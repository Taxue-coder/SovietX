//
//  ForwardToSelfPatch.mm
//  SovietX
//
//  ============================================================
//  撤回消息 → 同步发送给自己（全设备同步）
//  ============================================================
//
//  ★ 核心思路：在微信撤回回调里拿到原始消息内容，通过 SendMsg CGI
//    （sub_8da920）构造 type=5 文本消息发给自己的账号，全设备同步。
//
//  ★ 依赖（共 1 个 VM 地址）：
//     sub_8da920 (VA 0x8da920) — SendMsg CGI dispatcher
//     - Hopper: strings → "sendmsg_Send" / "send_msg_to_user is empty" 交叉引用定位
//     - x0 = 请求对象（80*8=640 字节），x1 = 1（发送标志）
//     - profile.sendMsgCGIVA 管理，YMSendMsgCGIRuntimeAddress() 取
//
//  ★ 请求对象布局（堆上构造，4 轮试错确认）：
//     +0x000: uint32_t type = 5
//     +0x120: std::string to       （接收方 wxid）
//     +0x138: std::string content  （消息正文）
//     +0x150: std::string from     （发送方 wxid）
//
//  ★ 重要修复（2026-06-30）：
//     旧版在本文件里通过 outWrap+0x30 / outWrap+0x48 猜 selfId。
//     开源用户反馈：部分环境下提醒消息没有发给自己，反而发给当前聊天对象。
//     根因就是 outWrap/origin message 的字段在不同场景下可能是当前会话、发送者或群 ID，
//     不能作为“当前登录账号”来源。
//     现在改为由 RevokePatch.mm 在“撤回消息 rawWrap”里读取 rawWrap+48，
//     作为 explicit selfUserText 显式传入；本文件只做安全校验，不再猜 selfId。
//     如果 selfUserText 不安全，直接跳过发送，宁可不发也不能误发给别人。
//
//  ★ MessageWrap 字段布局（616 字节，2026-06-26 日志确认）：
//     +0x18 (24)  = 会话展示名（私聊=对方号，群聊=群ID?）
//     +0x30 (48)  = 在撤回消息 rawWrap 中可作为当前登录账号 / 自己
//     +0x48 (72)  = 发送者 wxid
//     +0x100(256) = 毫秒创建时间
//     +0x108(264) = 消息类型 (originType)
//     +0x114(276) = 秒级创建时间
//     +0x130(304) = 消息内容 (文本=原文, 图片=CDN XML)
//     +0x148(328) = content/XML（另一偏移，可能冗余）
//     +0x160(352) = msgSource XML
//     +0x268(616) = 有效标志 (0=已删除)
//
//  ★ 群名获取（0 新增 hook/VM 地址，复用已有基础设施）：
//     YMCachedRoomName(roomID) 查缓存，未命中回退为“未知群聊”。
//
//  ★ 图片/视频/文件 转发（TODO，当前仅发文本通知）：
//     当前只发送提醒，不实际转发媒体内容。
//
//  ★ 门控：NSUserDefaults("kRevokeForwardToSelfRealSend.SOVIET")
//         或 /tmp/YMRevokeForwardToSelfRealSend 文件哨兵
//
//  ★ 日志：grep "RevokeAutoForward" /tmp/YMWeChatAntiRevokePatch.log
//

#import "ForwardToSelfPatch.h"
#import <objc/message.h>

#include <string>
#include <stdarg.h>
#include <new>

#pragma mark - 门控

BOOL YMRevokeRealSendForwardEnabled(void) {
    BOOL defaultsArmed = [[NSUserDefaults standardUserDefaults] boolForKey:@"kRevokeForwardToSelfRealSend.SOVIET"];
    BOOL fileArmed = [[NSFileManager defaultManager] fileExistsAtPath:@"/tmp/YMRevokeForwardToSelfRealSend"];
    return defaultsArmed || fileArmed;
}

#pragma mark - 日志

static void YMForwardLog(NSString *format, ...) {
    va_list args;
    va_start(args, format);
    NSString *msg = [[NSString alloc] initWithFormat:format arguments:args];
    va_end(args);

    NSLog(@"[YMAntiRevoke] [RevokeAutoForward] %@", msg ?: @"");

    NSString *line = [NSString stringWithFormat:@"[RevokeAutoForward] %@\n", msg ?: @""];
    NSData *data = [line dataUsingEncoding:NSUTF8StringEncoding];
    NSString *path = @"/tmp/YMWeChatAntiRevokePatch.log";

    NSFileHandle *fh = [NSFileHandle fileHandleForWritingAtPath:path];
    if (!fh) {
        [data writeToFile:path atomically:YES];
    } else {
        [fh seekToEndOfFile];
        [fh writeData:data];
        [fh closeFile];
    }
}

#pragma mark - 字符串 / 安全辅助

static NSString *YMForwardTrim(NSString *value) {
    if (value.length == 0) {
        return @"";
    }

    return [value stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] ?: @"";
}

static BOOL YMForwardStringMatchesPattern(NSString *value, NSString *pattern) {
    if (value.length == 0 || pattern.length == 0) {
        return NO;
    }

    NSError *error = nil;
    NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:pattern
                                                                           options:0
                                                                             error:&error];
    if (error || !regex) {
        return NO;
    }

    NSRange fullRange = NSMakeRange(0, value.length);
    NSTextCheckingResult *match = [regex firstMatchInString:value options:0 range:fullRange];
    return match && NSEqualRanges(match.range, fullRange);
}

static BOOL YMForwardLooksLikeAccountID(NSString *value) {
    NSString *text = YMForwardTrim(value);
    if (text.length == 0 || text.length > 128) {
        return NO;
    }

    if ([text containsString:@"@chatroom"] ||
        [text containsString:@"\n"] ||
        [text containsString:@" "] ||
        [text containsString:@"<"] ||
        [text containsString:@">"]) {
        return NO;
    }

    if ([text hasPrefix:@"wxid_"]) {
        return YES;
    }

    // 兼容老微信号 / 自定义微信号，例如 yanmaoweibo / MustangYM001。
    return YMForwardStringMatchesPattern(text, @"^[A-Za-z0-9_\\-]{5,128}$");
}

static BOOL YMForwardLooksLikeSafeSelfID(NSString *selfId,
                                         NSString *sessionText,
                                         NSString *revokerWxid,
                                         NSString *revokerDisplayName) {
    NSString *value = YMForwardTrim(selfId);
    NSString *session = YMForwardTrim(sessionText);
    NSString *revoker = YMForwardTrim(revokerWxid);

    if (!YMForwardLooksLikeAccountID(value)) {
        return NO;
    }

    // 绝对不能把目标设成群。
    if ([value containsString:@"@chatroom"]) {
        return NO;
    }

    // 私聊场景下，如果目标等于当前会话，极可能就是误把对方当自己。
    // 自己和自己的会话也可能相等，但宁可跳过，也不能误发给别人。
    if (session.length > 0 && [value isEqualToString:session]) {
        return NO;
    }

    // 如果 revoker 不是“你”，但 selfId 却等于 revoker，也高度可疑。
    // 这种情况可能是把撤回人/原发送者误当成当前登录账号。
    BOOL displayMeansMe = [revokerDisplayName isEqualToString:@"你"];
    if (revoker.length > 0 && [value isEqualToString:revoker] && !displayMeansMe) {
        return NO;
    }

    return YES;
}

#pragma mark - 格式化辅助

static BOOL YMForwardTextIsBuiltinEmoji(NSString *text) {
    NSString *value = YMForwardTrim(text);
    return value.length >= 3 && value.length <= 32 &&
           [value hasPrefix:@"["] && [value hasSuffix:@"]"] &&
           [value rangeOfString:@"\n"].location == NSNotFound;
}

static BOOL YMForwardTextLooksUseless(NSString *text) {
    if (text.length == 0) return NO;
    return [text containsString:@"暂不支持该内容"] ||
           [text containsString:@"请在手机上查看"];
}

/// 群聊消息格式为 wxid_xxx:\n内容，拆出发送者和正文
static NSString *YMForwardCleanContent(NSString *rawContent, NSString **senderOut) {
    if (senderOut) *senderOut = @"";
    if (rawContent.length == 0) return @"";

    NSString *text = YMForwardTrim(rawContent);
    NSRange colonNewline = [text rangeOfString:@":\n"];
    if (colonNewline.location != NSNotFound && colonNewline.location > 0) {
        NSString *prefix = [text substringToIndex:colonNewline.location];
        NSString *body   = [text substringFromIndex:NSMaxRange(colonNewline)];
        prefix = YMForwardTrim(prefix);
        body   = YMForwardTrim(body);
        if (prefix.length > 0 && senderOut) *senderOut = prefix;
        if (body.length > 0) return body;
    }

    return text;
}

static NSString *YMForwardContentDisplay(uint32_t type, NSString *cleanContent) {
    switch (type) {
        case 1:
            return (YMForwardTextIsBuiltinEmoji(cleanContent) && cleanContent.length)
                ? cleanContent
                : (cleanContent.length > 0 ? cleanContent : @"（空）");
        case 3:  return @"[图片]";
        case 34: return @"[语音]";
        case 43: return @"[视频]";
        case 47: return @"[表情包]";
        case 48: return @"[位置]";
        case 49: return @"[文件/链接/卡片]";
        default: return [NSString stringWithFormat:@"[%u]", type];
    }
}

static NSString *YMForwardRevokerDisplay(NSString *displayName, NSString *wxid, NSString *sender) {
    if (displayName.length > 0) return displayName;
    if (wxid.length > 0) return wxid;
    if (sender.length > 0) return sender;
    return @"***";
}

#pragma mark - 构建转发通知文本

static NSString *YMBuildRevokeForwardNotice(NSString *sessionText,
                                            uint32_t originType,
                                            NSString *originRawContent,
                                            NSString *revokerWxid,
                                            NSString *revokerDisplayName) {
    NSString *sender = @"";
    NSString *clean = originRawContent ?: @"";

    if (originType == 1) {
        clean = YMForwardCleanContent(originRawContent, &sender);
        if (YMForwardTextLooksUseless(clean)) {
            clean = @"";
        }
    }

    if (clean.length > 1600) {
        clean = [[clean substringToIndex:1600] stringByAppendingString:@"…"];
    }

    NSString *contentDisplay = YMForwardContentDisplay(originType, clean);
    NSString *revokerDisplay = YMForwardRevokerDisplay(revokerDisplayName, revokerWxid, sender);

    NSMutableString *notice = [NSMutableString string];
    [notice appendString:@"--拦截到一条撤回消息--\n"];

    if ([sessionText containsString:@"@chatroom"]) {
        NSString *roomName = YMCachedRoomName(sessionText);
        [notice appendFormat:@"群名:%@\n", roomName.length > 0 ? roomName : @"未知群聊"];
    }

    [notice appendFormat:@"撤回人:%@\n", revokerDisplay.length > 0 ? revokerDisplay : @"***"];
    [notice appendFormat:@"内容:%@", contentDisplay.length > 0 ? contentDisplay : @"（空）"];

    if (originType != 1) {
        [notice appendString:@"\n(非文字消息只做提醒)"];
    }

    NSDateFormatter *dateFmt = [[NSDateFormatter alloc] init];
    [dateFmt setDateFormat:@"yyyy-MM-dd HH:mm:ss"];
    NSString *timeStr = [dateFmt stringFromDate:[NSDate date]] ?: @"";
    if (timeStr.length > 0) {
        [notice appendFormat:@"\n%@", timeStr];
    }

    return notice;
}

#pragma mark - sub_8da920 type=5 发送

static BOOL YMForwardViaSendMsgCGI(NSString *selfId, NSString *content) {
    if (!selfId.length || !content.length) {
        return NO;
    }

    uintptr_t fn = YMSendMsgCGIRuntimeAddress();
    if (!fn) {
        YMForwardLog(@"SendMsg CGI runtime address is zero, skip");
        return NO;
    }

    uintptr_t obj[80] = {};
    *((uint32_t *)obj) = 5;

    std::string *toString = (std::string *)((uintptr_t)obj + 0x120);
    std::string *contentString = (std::string *)((uintptr_t)obj + 0x138);
    std::string *fromString = (std::string *)((uintptr_t)obj + 0x150);

    const char *selfUTF8 = [selfId UTF8String] ?: "";
    const char *contentUTF8 = [content UTF8String] ?: "";

    new (toString) std::string(selfUTF8);
    new (contentString) std::string(contentUTF8);
    new (fromString) std::string(selfUTF8);

    BOOL ok = YES;
    @try {
        ((int64_t(*)(uintptr_t, uintptr_t))fn)((uintptr_t)obj, 1);
    } @catch (...) {
        ok = NO;
    }

    fromString->~basic_string();
    contentString->~basic_string();
    toString->~basic_string();

    return ok;
}

#pragma mark - 统一入口

BOOL YMForwardToSelfSend(uintptr_t outWrap,
                         uint32_t originType,
                         NSString *originContent,
                         NSString *sessionText,
                         NSString *selfUserText,
                         NSString *revokerWxid,
                         NSString *revokerDisplayName) {
    (void)outWrap;

    NSString *selfId = YMForwardTrim(selfUserText);

    if (!YMForwardLooksLikeSafeSelfID(selfId, sessionText, revokerWxid, revokerDisplayName)) {
        YMForwardLog(@"unsafe selfId, skip real send. selfId=%@ session=%@ revoker=%@ displayName=%@ type=%u",
                     selfId ?: @"",
                     sessionText ?: @"",
                     revokerWxid ?: @"",
                     revokerDisplayName ?: @"",
                     originType);
        return NO;
    }

    NSString *notice = YMBuildRevokeForwardNotice(sessionText ?: @"",
                                                  originType,
                                                  originContent ?: @"",
                                                  revokerWxid ?: @"",
                                                  revokerDisplayName ?: @"");

    if (notice.length == 0) {
        YMForwardLog(@"notice is empty, skip real send. selfId=%@ session=%@", selfId ?: @"", sessionText ?: @"");
        return NO;
    }

    YMForwardLog(@"send revoke notice to self. selfId=%@ session=%@ type=%u noticeLen=%lu",
                 selfId ?: @"",
                 sessionText ?: @"",
                 originType,
                 (unsigned long)notice.length);

    return YMForwardViaSendMsgCGI(selfId, notice);
}
