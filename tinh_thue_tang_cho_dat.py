#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Tính thuế TNCN và lệ phí trước bạ khi tặng cho quyền sử dụng đất.

Căn cứ:
  - Luật Thuế TNCN số 109/2025/QH15 (hiệu lực 01/7/2026): thuế suất 10% trên
    phần giá trị vượt trên 20 triệu đồng theo từng lần phát sinh.
  - Nghị định 10/2022/NĐ-CP: lệ phí trước bạ nhà, đất 0,5%.
  - Miễn thuế TNCN và LPTB với quan hệ: vợ-chồng; cha/mẹ đẻ - con đẻ;
    cha/mẹ nuôi - con nuôi; cha/mẹ chồng - con dâu; cha/mẹ vợ - con rể;
    ông bà nội/ngoại - cháu nội/ngoại; anh, chị, em ruột.
    KHÔNG bao gồm quan hệ cha dượng/mẹ kế - con riêng.
"""

from decimal import Decimal

NGUONG_MIEN_TRU = Decimal("20000000")   # 20 triệu đồng / lần phát sinh
THUE_SUAT_TNCN = Decimal("0.10")        # 10%
TY_LE_LPTB = Decimal("0.005")           # 0,5%

# Các quan hệ được miễn thuế TNCN và lệ phí trước bạ khi tặng cho BĐS
QUAN_HE_MIEN = {
    "vo_chong", "cha_de_con_de", "me_de_con_de", "cha_nuoi_con_nuoi",
    "me_nuoi_con_nuoi", "cha_me_chong_con_dau", "cha_me_vo_con_re",
    "ong_ba_noi_chau_noi", "ong_ba_ngoai_chau_ngoai", "anh_chi_em_ruot",
}


def tinh_cho_mot_nguoi(gia_tri_nhan, ty_le_so_huu_ben_tang, quan_he_voi_ben_tang):
    """Tính nghĩa vụ tài chính của MỘT người nhận tặng cho.

    gia_tri_nhan: tổng giá trị phần đất người này nhận (theo Bảng giá đất).
    ty_le_so_huu_ben_tang: dict {ten_ben_tang: ty_le_so_huu_trong_thua_dat}.
    quan_he_voi_ben_tang: dict {ten_ben_tang: ma_quan_he}.
    """
    gia_tri_nhan = Decimal(str(gia_tri_nhan))
    phan_chiu_thue = Decimal("0")
    chi_tiet = {}

    for ben_tang, ty_le in ty_le_so_huu_ben_tang.items():
        phan_tu_ben_tang = gia_tri_nhan * Decimal(str(ty_le))
        duoc_mien = quan_he_voi_ben_tang.get(ben_tang) in QUAN_HE_MIEN
        chi_tiet[ben_tang] = {
            "gia_tri": phan_tu_ben_tang,
            "duoc_mien": duoc_mien,
        }
        if not duoc_mien:
            phan_chiu_thue += phan_tu_ben_tang

    thu_nhap_tinh_thue = max(Decimal("0"), phan_chiu_thue - NGUONG_MIEN_TRU)
    thue_tncn = thu_nhap_tinh_thue * THUE_SUAT_TNCN
    le_phi_truoc_ba = phan_chiu_thue * TY_LE_LPTB

    return {
        "gia_tri_nhan": gia_tri_nhan,
        "phan_chiu_thue": phan_chiu_thue,
        "thu_nhap_tinh_thue": thu_nhap_tinh_thue,
        "thue_tncn": thue_tncn,
        "le_phi_truoc_ba": le_phi_truoc_ba,
        "tong_phai_nop": thue_tncn + le_phi_truoc_ba,
        "chi_tiet_theo_ben_tang": chi_tiet,
    }


def vnd(x):
    return f"{int(round(float(x))):,}".replace(",", ".") + " đ"


def main():
    # ---- Dữ liệu đầu vào (sửa theo thực tế thửa đất) -------------------
    dien_tich = Decimal("200")                 # m2
    don_gia = Decimal("5000000")               # đ/m2 theo Bảng giá đất Hà Tĩnh 2026
    gia_tri_thua_dat = dien_tich * don_gia

    # Thửa đất là tài sản chung của vợ chồng -> mỗi người 50%
    so_huu = {"bo": Decimal("0.5"), "me_ke": Decimal("0.5")}

    # Tặng đều cho 3 con gái
    nguoi_nhan = [
        ("Con gái 1 (con chung)", Decimal("1") / 3,
         {"bo": "cha_de_con_de", "me_ke": "me_de_con_de"}),
        ("Con gái 2 (con riêng của chồng)", Decimal("1") / 3,
         {"bo": "cha_de_con_de", "me_ke": "me_ke_con_rieng"}),
        ("Con gái 3 (con riêng của chồng)", Decimal("1") / 3,
         {"bo": "cha_de_con_de", "me_ke": "me_ke_con_rieng"}),
    ]
    # -------------------------------------------------------------------

    print(f"Giá trị thửa đất (theo Bảng giá đất): {vnd(gia_tri_thua_dat)}")
    print(f"Tỷ lệ sở hữu: bố {so_huu['bo']} - mẹ kế {so_huu['me_ke']}\n")

    tong = Decimal("0")
    for ten, ty_le_duoc_tang, quan_he in nguoi_nhan:
        kq = tinh_cho_mot_nguoi(
            gia_tri_thua_dat * ty_le_duoc_tang, so_huu, quan_he
        )
        print(f"── {ten}")
        print(f"   Giá trị được tặng      : {vnd(kq['gia_tri_nhan'])}")
        for ben, ct in kq["chi_tiet_theo_ben_tang"].items():
            trang_thai = "MIỄN" if ct["duoc_mien"] else "CHỊU THUẾ"
            print(f"     • từ {ben:<7}: {vnd(ct['gia_tri']):>18}  [{trang_thai}]")
        print(f"   Phần chịu thuế         : {vnd(kq['phan_chiu_thue'])}")
        print(f"   Thu nhập tính thuế     : {vnd(kq['thu_nhap_tinh_thue'])}"
              f"  (đã trừ {vnd(NGUONG_MIEN_TRU)})")
        print(f"   Thuế TNCN (10%)        : {vnd(kq['thue_tncn'])}")
        print(f"   Lệ phí trước bạ (0,5%) : {vnd(kq['le_phi_truoc_ba'])}")
        print(f"   TỔNG PHẢI NỘP          : {vnd(kq['tong_phai_nop'])}\n")
        tong += kq["tong_phai_nop"]

    print(f"TỔNG CỘNG CẢ 3 NGƯỜI: {vnd(tong)}")


if __name__ == "__main__":
    main()
