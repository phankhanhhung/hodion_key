// Khai báo chung cho TSF text service của HodionKey.
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <msctf.h>

#include "TsfCompat.h"

// ---- GUID của text service (định nghĩa trong Guids.cpp) -------------------
extern const CLSID CLSID_HodionKeyService;
extern const GUID GUID_HodionKeyProfile;
extern const GUID GUID_HodionKeyDisplayAttributeInput;
extern const GUID GUID_HodionKeyToggle;

// vi-VN
constexpr LANGID kHodionLangId = MAKELANGID(LANG_VIETNAMESE, SUBLANG_DEFAULT);

constexpr WCHAR kServiceDescription[] = L"HodionKey";
constexpr WCHAR kToggleKeyDescription[] = L"HodionKey: bat/tat tieng Viet";
constexpr WCHAR kReconversionName[] = L"HodionKey: chon lai dau";

// ---- Trạng thái module ----------------------------------------------------
extern HINSTANCE g_hInst;

void DllAddRef();
void DllRelease();

// ---- Đăng ký (Register.cpp) ----------------------------------------------
HRESULT RegisterComServer();
void UnregisterComServer();
HRESULT RegisterProfiles();
void UnregisterProfiles();
HRESULT RegisterCategories();
void UnregisterCategories();

// ---- COM smart pointer tối giản ------------------------------------------
template <typename T>
class com_ptr {
 public:
  com_ptr() = default;
  com_ptr(const com_ptr&) = delete;
  com_ptr& operator=(const com_ptr&) = delete;
  ~com_ptr() { reset(); }

  T* get() const { return p_; }
  T** put() {
    reset();
    return &p_;
  }
  void** put_void() { return reinterpret_cast<void**>(put()); }
  T* operator->() const { return p_; }
  explicit operator bool() const { return p_ != nullptr; }

  void reset() {
    if (p_) {
      p_->Release();
      p_ = nullptr;
    }
  }

  // Nhận con trỏ đã AddRef sẵn (từ tham số out của COM).
  void attach(T* p) {
    reset();
    p_ = p;
  }
  T* detach() {
    T* p = p_;
    p_ = nullptr;
    return p;
  }
  void copy_from(T* p) {
    reset();
    p_ = p;
    if (p_) p_->AddRef();
  }

 private:
  T* p_ = nullptr;
};
