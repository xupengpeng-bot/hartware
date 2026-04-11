#ifndef VOICE_CHIP_PROMPTS_H
#define VOICE_CHIP_PROMPTS_H

typedef struct {
    unsigned char chip_code;
    const char   *phrase;
} voice_chip_prompt_t;

#define VOICE_CHIP_PROMPT_TABLE(X) \
    X(2,  "零") \
    X(3,  "一") \
    X(4,  "二") \
    X(5,  "三") \
    X(6,  "四") \
    X(7,  "五") \
    X(8,  "六") \
    X(9,  "七") \
    X(10, "八") \
    X(11, "九") \
    X(12, "十") \
    X(13, "百") \
    X(14, "千") \
    X(15, "万") \
    X(16, "点") \
    X(17, "元") \
    X(18, "欢迎使用") \
    X(19, "灌溉已暂停") \
    X(20, "卡内余额") \
    X(21, "本次使用金额") \
    X(22, "卡内剩余金额") \
    X(23, "支付宝收款") \
    X(24, "微信收款") \
    X(25, "无效卡") \
    X(26, "卡余额不足") \
    X(27, "端口已占用") \
    X(28, "启动中请稍后") \
    X(29, "电表故障") \
    X(30, "端口已占用") \
    X(31, "此卡已停用") \
    X(32, "设备故障") \
    X(33, "灌溉已恢复") \
    X(34, "开始灌溉") \
    X(35, "结束灌溉") \
    X(36, "无法使用")

#endif /* VOICE_CHIP_PROMPTS_H */
