# Luật cho PSScriptAnalyzer, dùng cả ở CI lẫn lúc chạy tay:
#
#   Invoke-ScriptAnalyzer -Path platform/windows/dist -Recurse `
#       -Settings platform/windows/dist/PSScriptAnalyzerSettings.psd1
#
# Bốn luật bị tắt, mỗi cái một lý do cụ thể chứ không phải cho đỡ ồn:
#
#   PSAvoidUsingWriteHost      — đây là bộ cài chạy trong cửa sổ console,
#                                Write-Host CHÍNH LÀ giao diện của nó. Đổi
#                                sang Write-Output là hỏng: chuỗi trả về
#                                sẽ lẫn vào giá trị của hàm.
#   PSAvoidUsingEmptyCatchBlock— các catch rỗng đều là dọn dẹp "được thì
#                                tốt": đóng tiến trình đã chết, xoá file
#                                tạm không còn. Báo lỗi ở đó chỉ làm nhiễu
#                                đúng lúc người dùng đang cần đọc lỗi thật.
#   PSUseShouldProcess...      — Stop-Tray/Start-Tray là bước trong một
#                                kịch bản cài đặt tuyến tính, không phải
#                                cmdlet cho người khác gọi lại.
#   PSReviewUnusedParameter    — báo nhầm: các tham số switch được đọc
#                                trong hàm con qua biến phạm vi script,
#                                phân tích tĩnh không lần ra được.
@{
    Severity     = @('Error', 'Warning')
    ExcludeRules = @(
        'PSAvoidUsingWriteHost',
        'PSAvoidUsingEmptyCatchBlock',
        'PSUseShouldProcessForStateChangingFunctions',
        'PSReviewUnusedParameter'
    )
}
