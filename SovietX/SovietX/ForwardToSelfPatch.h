//
//  ForwardToSelfPatch.h
//  SovietX
//  Thanks Code by https://github.com/shay-wong

//  撤回消息 → 同步发送给自己（全设备同步）
//
//  依赖：
//    1 VM 地址 — sub_8da920 (SendMsg CGI)，profile.sendMsgCGIVA 管理
//    RevokePatch 中 UpdateSessionCache hook 提供的群名缓存（YMCachedRoomName）
//
//  思路：撤回回调里拿到原始消息内容，构造 type=5 文本消息通过 SendMsg CGI
//  发给自己。群聊群名从缓存取，未命中回退为“未知群聊”。0 新增 hook 点。
//

#import <Foundation/Foundation.h>
#import <stdint.h>

/// sub_8da920 SendMsg CGI 运行时地址
uintptr_t YMSendMsgCGIRuntimeAddress(void);

/// 通过 roomID 查群名，查不到返回 @""
NSString *YMCachedRoomName(NSString *roomID);

BOOL YMRevokeRealSendForwardEnabled(void);

/// 撤回消息 → 文本通知发送给自己。
BOOL YMForwardToSelfSend(uintptr_t outWrap,
                         uint32_t  originType,
                         NSString *originContent,
                         NSString *sessionText,
                         NSString *selfUserText,
                         NSString *revokerWxid,
                         NSString *revokerDisplayName);

