/* C ABI của engine — dùng cho FFI (Swift trên macOS, hoặc host C thuần).
 * Chuỗi vào/ra đều là UTF-8. */
#ifndef HODION_ENGINE_C_H
#define HODION_ENGINE_C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hodion_engine hodion_engine;

enum hodion_method { HODION_METHOD_TELEX = 0, HODION_METHOD_VNI = 1 };
enum hodion_tone_style { HODION_TONE_TRADITIONAL = 0, HODION_TONE_MODERN = 1 };
enum hodion_action {
  HODION_ACTION_NONE = 0,      /* phím không thuộc engine — host tự chèn */
  HODION_ACTION_COMPOSING = 1, /* cập nhật composition = out */
  HODION_ACTION_COMMIT = 2     /* chốt out rồi kết thúc composition */
};

hodion_engine* hodion_engine_create(void);
void hodion_engine_destroy(hodion_engine* e);

void hodion_engine_set_method(hodion_engine* e, int method);
void hodion_engine_set_tone_style(hodion_engine* e, int style);

/* Đẩy một codepoint (đã phân giải shift/caps). Kết quả UTF-8 ghi vào out
 * (NUL-terminated, cắt bớt nếu thiếu chỗ). Trả về hodion_action. */
int hodion_engine_key(hodion_engine* e, uint32_t codepoint, char* out,
                      size_t out_cap);
int hodion_engine_backspace(hodion_engine* e, char* out, size_t out_cap);

/* Chuỗi hiển thị hiện tại (trả về số byte cần, không tính NUL). */
size_t hodion_engine_composition(const hodion_engine* e, char* out,
                                 size_t out_cap);
int hodion_engine_composing(const hodion_engine* e);
void hodion_engine_reset(hodion_engine* e);

#ifdef __cplusplus
}
#endif

#endif /* HODION_ENGINE_C_H */
