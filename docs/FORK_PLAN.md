# FORK_PLAN: Jenny Script (fork của Story Architect)

> Kế hoạch làm việc cho Claude Code và Kha. Làm lần lượt từng phase. Cuối mỗi phiên, cập nhật mục **Trạng thái** ở cuối file.
> Viết ngày 03/11/2026, dựa trên nhánh `master` của `story-apps/starc` (v0.8.4).

## 1. Mục tiêu
Studio cần một app viết kịch bản kiểu Celtx, có thêm:
1. **Cầu nối Google Docs.** Đẩy kịch bản lên Google Docs để cả team cùng sửa và comment, sau đó kéo bản đã sửa về app. Nếu team đang viết kịch bản dạng text Fountain trên Google Docs (ví dụ dàn trang bằng extension **Fountainize**; xác nhận ở D5) thì chế độ Fountain là ưu tiên số 1.
2. **Project online và share.** Lưu/mở project trên Google Drive, chia sẻ cho người trong team.
3. Làm sau, tùy chọn: cộng tác realtime ngay trong app, giao diện tiếng Việt, dựng lại vài plugin PRO cần thiết.

**Hướng chính: dùng Google Drive/Docs làm lớp "online".** Không phải tự chạy server, tận dụng luôn quyền share và lịch sử phiên bản của Google, lại khớp với workflow Google Docs hiện tại.

## 2. Giới hạn cần biết trước
- Fork **không có** phần cloud gốc và khoảng 25 plugin PRO (đều là submodule private): không có cards, timeline, mind map, breakdown, statistics, hồ sơ nhân vật/bối cảnh, collaborators… Bản build từ source gồm các trình soạn thảo (screenplay, treatment, novel, comic, stage/audio play, simple text, title page), navigator và import/export.
- `CloudServiceManager` gốc có khoảng 95 hàm/signal (account, team, project, document changes, cursors, AI, payment). Viết lại cho tương thích rồi dựng server là dự án nhiều tháng, nên để ở Phase 3 và chỉ làm khi thật sự cần.
- Viết chung realtime thì Google Docs đã có sẵn (thông qua Phase 1). Nếu cần realtime *bên trong app*, phương án rẻ nhất là gói Creator của STARC gốc (khoảng 8,34 USD/tháng khi trả theo năm, giá ngày 03/11/2026).

## 3. Quyết định Kha cần chốt (hỏi ngay đầu Phase 0)
| # | Câu hỏi | Đề xuất mặc định |
|---|---|---|
| D1 | Email studio có chạy trên **Google Workspace** không? | **Có:** đặt OAuth consent screen ở chế độ "Internal". Không bị giới hạn 100 test user, không phải đăng nhập lại mỗi 7 ngày, dùng được scope `drive` trong nội bộ (admin Workspace có thể phải cho phép app ở mục API controls). **Không:** chế độ "External". Khi app ở trạng thái Testing thì tối đa 100 test user, cứ 7 ngày phải đăng nhập lại. Muốn bỏ giới hạn thì Publish app; nếu chỉ dùng scope non-sensitive như `drive.file` thì chỉ cần verification cơ bản. |
| D2 | Fork public (nút Fork trên GitHub) hay repo private? | **Public fork.** Sync upstream dễ, và khi phát app cho freelancer/khách thì vẫn đúng GPL. Chỉ chọn private khi app dùng hoàn toàn trong studio. |
| D3 | Tên app và icon | "Jenny Script" (tên tạm) |
| D4 | User test trên Windows hay macOS? | (chưa có) |
| D5 | Team đang viết kịch bản trên Google Docs thế nào: text Fountain (vd. với Fountainize) hay tự format tay? Scene heading viết kiểu nào: `INT./EXT.` hay `NỘI./NGOẠI.`? Có gạch ngang giữa địa điểm và thời gian không? Xin user 1 file kịch bản mẫu. | Hỗ trợ cả hai kiểu heading |
| D6 | Có bỏ Crashpad trong bản fork không? | Có. Fork không gửi crash về upstream, và bỏ Crashpad thì build không cần depot_tools. |

## 4. Phase 0: Nền móng
**Việc cần làm**
- 0.1 Hỏi D1–D6, ghi kết quả vào mục Trạng thái.
- 0.2 Cấu hình remote: `origin` là repo của Kha, `upstream` là `https://github.com/story-apps/starc`. Ghi cách sync upstream vào `README_FORK.md`: `git fetch upstream && git merge upstream/master`, làm mỗi tháng hoặc khi upstream ra bản mới.
- 0.3 Build bản Linux trong môi trường của Claude Code (hoặc WSL) để kiểm lỗi compile. Hướng dẫn Kha tự build trên máy, hoặc tải bản build từ CI (theo D4).
- 0.4 Viết CI riêng `.github/workflows/build_fork.yml` cho Windows và macOS (thêm Linux AppImage nếu được):
  - chỉ init submodule public;
  - Qt 6.11.x có thêm `qtnetworkauth`;
  - upload artifact (installer, zip, dmg);
  - bỏ các bước BugSplat symbol upload, signing, publish release của upstream.

  Các workflow upstream cần secrets và submodule private nên sẽ fail trong fork. Tắt chúng bằng GitHub UI (Actions → chọn workflow → Disable) thay vì xóa file, để merge upstream không bị conflict.
- 0.5 Rebrand theo D3: tên app, organization, domain trong `src/app/application.cpp`; icon; bundle id macOS; tên installer (NSIS, AppImage). Đổi tên để QSettings và thư mục data tách hẳn khỏi bản STARC chính thức, phòng khi máy cài cả hai. Trong About giữ dòng "Based on Story Architect by Story Apps".
- 0.6 Tắt gửi crash lên BugSplat. Liệt kê các endpoint upstream mà app còn gọi (fonts, từ điển hunspell, unsplash, gproxy, logline help, link help/blog/mạng xã hội), rồi hỏi Kha giữ, tắt hay ẩn từng cái.
- 0.7 (theo D6) Thêm tùy chọn qmake `CONFIG+=no_crashpad`: bỏ link Crashpad, bọc code khởi tạo bằng `#ifndef NO_CRASHPAD`.
- 0.8 Viết `README_FORK.md`: fork này là gì, khác upstream ở đâu, cách build, ghi chú GPL, danh sách file upstream đã sửa.
- 0.9 So với bản STARC Free chính thức, lập bảng tính năng bị thiếu ở bản build từ source. User dựa vào đó chọn cái nào đáng dựng lại (Phase 5).

**Nghiệm thu (Kha test tay)**
- [ ] Tải artifact từ GitHub Actions, cài và mở được trên máy của user. Bản chưa ký số: macOS phải chuột phải → Open; Windows chọn "More info → Run anyway".
- [ ] Tạo project, viết vài cảnh tiếng Việt có dấu, lưu, đóng rồi mở lại không lỗi.
- [ ] Import được `.fountain`, `.fdx`, `.celtx`; export PDF, DOCX, Fountain đúng format.
- [ ] App hiện tên mới; settings không dính vào bản STARC chính thức.
- [ ] Không còn request nào tới `bugsplat.com`.

## 5. Phase 1: Cầu nối Google Docs (MVP)
**Kiến trúc**
- Module mới `src/core/management_layer/content/google/` (chỉnh lại cho đúng convention thực tế của repo), gồm:
  - `GoogleAuthManager`: đăng nhập/đăng xuất, refresh token, trạng thái tài khoản (email).
  - `GoogleDriveClient`: lớp REST mỏng trên `QNetworkAccessManager`. Tạo folder; upload multipart/resumable có convert; update media; đọc metadata (`id,name,mimeType,modifiedTime,version,webViewLink,appProperties`); `files.export`; list theo query. Lỗi 429/5xx thì retry với exponential backoff; gặp `invalid_grant` thì yêu cầu đăng nhập lại.
  - `GoogleDocsBridge`: dùng exporter/importer có sẵn. Đẩy lên: export ra file tạm rồi upload. Kéo về: export Doc ra file tạm rồi import.
- Toàn bộ module bọc trong `GOOGLE_INTEGRATION`, để build thiếu `networkauth` vẫn chạy được.
- Mapping giữa UUID document và Google file ID (kèm chế độ đẩy, hash nội dung và thời điểm của lần đẩy/kéo cuối) lưu trong QSettings, không đụng vào file `.starc`.

**Tính năng**
- Settings có thêm mục **Google**: Đăng nhập / Đăng xuất, hiển thị email đang dùng.
- Menu của document kịch bản có thêm **Google Docs ▸** với các lệnh:
  - **Đẩy lên (Fountain, sửa được):** `ScreenplayFountainExporter` xuất file, upload dạng `text/plain` có convert thành Google Doc trong thư mục `Jenny Script/<tên project>/`. Các lần sau cập nhật đúng Doc đó.
  - **Đẩy bản đẹp để duyệt:** `ScreenplayDocxExporter` xuất DOCX rồi convert thành Google Doc giữ nguyên format (hoặc đẩy PDF cho người chỉ đọc). Bản này chỉ đi một chiều, không kéo về.
  - **Kéo về:** `files.export text/plain` rồi đưa qua `ScreenplayFountainImporter`, tạo thành **draft mới** tên "Google Docs dd/mm HH:mm", không ghi đè bản đang có. API draft: xem `draftSource()` và `showViewForDraft()` trong `src/core/management_layer/content/project/project_manager.h`. Nếu tạo draft bằng code quá rối thì nhập thành document mới trong project.
  - **Mở trên trình duyệt:** mở `webViewLink`.
- Trước khi đẩy, kiểm xem Doc có bị sửa trên Google Docs chưa: export `text/plain`, tính SHA-256, so với hash của lần sync cuối (`version`/`modifiedTime` chỉ dùng để lọc nhanh). Nếu đã bị sửa thì hỏi user: Kéo về trước / Ghi đè / Hủy.
- (theo D5) Scene heading tiếng Việt: thêm từ khóa như `NỘI`, `NGOẠI`, `NỘI/NGOẠI` vào `sceneHeadingsDictionary()`, hoặc đưa vào bản dịch `vi` (Phase 4), để importer nhận ra.

**Việc Kha cần làm (Claude Code hướng dẫn từng bước)**
1. Tạo Google Cloud project và bật **Google Drive API**.
2. Cấu hình OAuth consent screen theo D1 (Internal; hoặc External rồi thêm test user).
3. Tạo OAuth client loại **Desktop app**, điền client ID/secret vào file `src/google_oauth.pri` trên máy (file này đã gitignore). Khi build trên CI thì dùng GitHub Secrets.

**Test tự động**
- Test `GoogleDriveClient` với `QNetworkAccessManager` giả (không gọi mạng thật): upload, update, export, các lỗi 401/403/404/429.
- Test round-trip: export Fountain rồi import lại kịch bản mẫu tiếng Việt (theo D5), so số scene, tên nhân vật, thoại và dấu tiếng Việt.

**Nghiệm thu (Kha test tay)**
- [ ] Đăng nhập Google; tắt app mở lại vẫn còn đăng nhập.
- [ ] Đẩy kịch bản lên thì Doc nằm đúng thư mục; chạy Fountainize trên Doc ra đúng format.
- [ ] Sửa trên Google Docs (thêm cảnh, sửa thoại) rồi Kéo về: draft mới đúng scene, nhân vật, thoại, dấu tiếng Việt nguyên vẹn.
- [ ] Đẩy lần 2 khi Doc đã bị người khác sửa thì có cảnh báo, không ghi đè âm thầm.
- [ ] Bản đẹp để duyệt giữ được format (Courier, thụt lề, tên nhân vật in hoa).
- [ ] Mất mạng hoặc token hết hạn thì báo lỗi dễ hiểu, không crash, không mất dữ liệu.

**Rủi ro, cần kiểm sớm**
- Fountainize có giữ nguyên ký tự Fountain (`.`, `@`, `>`, `~`…) trong text khi export không? Điều này quyết định chất lượng khi kéo về. Test bằng Doc thật của Kha.
- Upload đè nội dung Doc có làm mất comment không? Nếu mất thì có hai cách: mỗi lần đẩy tạo một Doc phiên bản mới (Doc cũ đổi tên thành "… (cũ dd/mm)"), hoặc chặn đẩy khi Doc còn comment chưa resolve.
- Với scope `drive.file`, app chỉ thấy file do chính nó tạo hoặc file user chọn qua Google Picker. Doc tạo sẵn bên ngoài app vì vậy chưa kéo về được, xử lý ở Phase 1b.

### Phase 1b: Mở Google Doc có sẵn (tạo bên ngoài app)
- **Nếu D1 = có Workspace:** app để chế độ Internal, xin thêm scope `drive.readonly` hoặc `drive`. Đây là scope restricted, nhưng app nội bộ không cần verify (admin có thể phải trust app). Khi đó chỉ cần dán link Doc là kéo về được.
- **Nếu D1 = không có Workspace:** chạy Google Picker trong trình duyệt hệ thống. App mở một trang HTML local (qua `QHttpServer` hoặc server loopback), Picker trả file ID về cho app, và file đó được cấp quyền `drive.file`. Cần API key và App ID (project number).
- **Cách không cần code:** trên Google Docs chọn File → Download → `.txt` hoặc `.docx`, rồi Import vào app như bình thường.

## 6. Phase 2: Project online qua Google Drive
**Tính năng**
- **Lưu lên Drive:** tạo snapshot nhất quán bằng `VACUUM INTO '<tmp>.starc'` (hoặc SQLite backup API), không upload thẳng file đang mở. Upload snapshot vào `Jenny Script/<project>/`, gắn `appProperties`: `jsProject=1`, `projectUuid`, `appVersion`.
- **Mở từ Drive:** tải về cache local và làm việc offline. **Đồng bộ** nghĩa là đẩy một snapshot mới lên.
- **Chống ghi đè:** lưu `headRevisionId` (hoặc `md5Checksum`) của lần sync cuối. Không dùng `version`, vì cập nhật `appProperties` (để khóa) cũng làm `version` tăng. Trước khi đẩy, đọc metadata; nếu khác thì tải bản trên Drive về thành "bản xung đột", mở song song để user chọn giữ bản nào.
- **Khóa mềm:** ghi vào `appProperties` hai trường `lockedBy` (email) và `lockedUntil` (gia hạn khoảng 5 phút một lần khi đang mở). Người khác mở sẽ thấy "<email người đang mở> đang sửa, mở chỉ đọc?". Khóa hết hạn thì tự nhả.
- **Share:** nút mở trang chia sẻ của Drive trên trình duyệt (cách đơn giản nhất); tùy chọn thêm `permissions.create` ngay trong app.
- **Người được share mở project:** nếu D1 = có Workspace thì liệt kê "Shared with me" có `jsProject=1` (cần scope `drive` hoặc `drive.readonly`); nếu không có Workspace thì dùng Google Picker (như Phase 1b).
- (Tùy chọn) **Khôi phục bản cũ** từ lịch sử revisions của Drive.

**Nghiệm thu**
- [ ] Máy A lưu lên Drive; máy B (tài khoản khác, đã được share) mở được.
- [ ] A đang mở thì B thấy khóa, và mở được ở chế độ chỉ đọc.
- [ ] A và B cùng sửa offline rồi đồng bộ: có bản xung đột, không mất dữ liệu của bên nào.
- [ ] Project lớn (nhiều ảnh) upload được bằng resumable upload, có hiện tiến trình.

## 7. Phase 3 (tùy chọn, việc lớn): Cộng tác realtime trong app
Chỉ bắt đầu khi Kha xác nhận Phase 1–2 không đủ dùng **và** đã cân nhắc mua gói Creator của STARC gốc.
- Đọc mọi chỗ gọi `cloudServiceManager` trong `application_manager.cpp` để suy ra interface tối thiểu: login, danh sách project, mở/đóng document, `pushDocumentChange` / `documentChanged`, `updateCursor` / `cursorsChanged`, `projectCantBeSynced`.
- Viết `CloudServiceManager` thay thế trong `src/cloud/` của fork (phải đổi submodule thành thư mục thường, hỏi Kha trước). Dựng server riêng (WebSocket; client đã có `qtwebsockets`), xác thực bằng Google ID token, lưu change log của từng document.
- Dùng `tests/remote_text_changes/` làm chuẩn cho việc merge thay đổi từ xa.
- Phase này cần dev backend thực thụ và hosting. Ước lượng nhiều tháng.

## 8. Phase 4 (tùy chọn): Tiếng Việt
- Tạo `src/core/translations/translation_vi.ts` bằng `lupdate`, thêm vào `TRANSLATIONS` trong `src/starc.pro`, dịch dần (ưu tiên menu chính, settings và các mục Google). Có thể đóng góp ngược lại qua Transifex của upstream.
- Trong bản `vi`, dịch các chuỗi `INT`/`EXT`… của `AbstractFountainImporter` thành `NỘI`/`NGOẠI` (theo D5).
- Thêm từ điển chính tả tiếng Việt (hunspell) nếu có.

## 9. Phase 5 (tùy chọn): Dựng lại plugin cần thiết
Data model đã public (`src/corelib/business_layer/model/characters`, `locations`, `worlds`, `mind_map`…), chỉ có UI plugin là private. Dựa trên bảng ở task 0.9, Kha chọn plugin cần dựng lại (gợi ý: hồ sơ nhân vật, hồ sơ bối cảnh, cards). Mỗi plugin làm trên một branch, lấy các plugin public `*_structure` và `screenplay_text` làm mẫu.

## 10. Checklist GPL-3.0 (không phải tư vấn pháp lý)
- Giữ `LICENSE` và header bản quyền. File đã sửa thì ghi chú "Modified by Jenny Pixel Studio, <năm>".
- Dùng nội bộ trong studio thì không bắt buộc công bố source. Phát bản build cho người ngoài (freelancer, khách) thì phải kèm source, hoặc chỉ chỗ lấy source tương ứng, theo GPL-3.0 (repo public là đủ).
- Không dùng tên/logo "Story Architect" hay "STARC" làm tên cho app fork.
- Thư viện thêm vào phải tương thích GPL-3.0 (MIT, BSD, LGPL, Apache-2.0 đều được).

## 11. Trạng thái
**Quyết định (chốt 03/11/2026):**
- D1 = **không** có Google Workspace (Gmail thường): OAuth chế độ External, scope `drive.file`; Phase 1b dùng Google Picker.
- D2 = **public fork**: https://github.com/vienglacial/starc
- D3 = **Jenny Script** (icon tạm giữ của upstream, studio gửi sau).
- D4 = **Windows** x64.
- D5 = team viết theo chuẩn kịch bản quốc tế; khi đưa lên Google Docs thì dùng text Fountain dàn trang bằng Fountainize, nên chế độ Fountain là ưu tiên số 1. Scene heading nhận **cả hai** kiểu `INT./EXT.` và `NỘI./NGOẠI.`. Chưa có file mẫu.
- D6 = **bỏ Crashpad** (`CONFIG+=no_crashpad`).
- Build thử bằng **GitHub Actions** (máy dev chưa có Qt/MSVC/WSL).
- 0.6 endpoint: **tắt** telemetry `telemetry.storyapps.dev` (đã làm); crash BugSplat tắt bằng `no_crashpad`. **Giữ**: Unsplash, gproxy (lưu ý: text đi qua proxy HTTP lạ, chỉ dùng ở cover generator), logline help, tải font, từ điển hunspell, link Help. **Ẩn** link blog/contacts/storyapps.dev (làm cùng 0.5, trong About/menu).
- Workflow upstream chưa được GitHub đăng ký trong fork, nên chưa tắt được. Khi merge vào `master` lần đầu thì chạy `gh workflow disable` cho `build_starc_linux.yml`, `build_starc_mac.yml`, `build_starc_windows.yml`, `clang-format.yml`.

| Phase | Trạng thái | Branch | Ghi chú |
|---|---|---|---|
| 0 Nền móng | đang làm | `phase-0-foundation` | xong: 0.1, 0.2, 0.7, bản đầu 0.4/0.6/0.8. CI lần đầu: Windows build OK (zip portable), Linux build OK, test 41/42 (1 ca lệch 1px đã đưa vào BLACKLIST) |
| 1 Google Docs | đang làm: code xong, CI xanh, chờ Kha test tay | `phase-1-gdocs` | đăng nhập (onboarding + Settings), đẩy lên / kéo về draft / mở trình duyệt; sửa Fountain importer (số cảnh, `..`, NỘI/NGOẠI) |
| 1b Doc có sẵn | chưa bắt đầu | | |
| 2 Project trên Drive | chưa bắt đầu | | |
| 3 Realtime | tạm hoãn | | |
| 4 Tiếng Việt | đang làm (chen trước Phase 0 theo yêu cầu) | `phase-4-vietnamese` | dịch đủ 4.129 chuỗi bằng AI, chờ build CI và team rà thuật ngữ |
| 5 Plugin | tùy chọn | | |

**Nhật ký** (mới nhất ở trên cùng). Mỗi dòng ghi: `dd/mm/yyyy — đã làm gì — việc tiếp theo`
- 06/11/2026 — Kiểm tra bản Mac sau phản hồi "đầy bug": gói `.dmg` đủ plugin Qt (cocoa, tls, multimedia, sqlite, ảnh), 39 plugin app giống Windows, log mở app sạch. Lỗi chung cho mọi hệ: nhiều mục trỏ tới plugin private (nhân vật, bối cảnh, thế giới, thẻ cảnh, timeline, breakdown, sơ đồ tư duy, thư viện ảnh, thuyết trình, kịch bản chia tập) bấm vào trống. Giờ ẩn theo thực tế có plugin hay không: `PluginsBuilder::documentMimeTypesWithoutEditors()` → `StructureProxyModel::setHiddenDocumentTypes()` và `CreateDocumentDialog::setHiddenDocumentTypes()`; icon view thiếu plugin bị bỏ khỏi thanh công cụ. Chưa có danh sách lỗi cụ thể từ thầy của Kha.
- 06/11/2026 — Doc giống trang soạn thảo hơn: có số cảnh, dải xám ở tiêu đề cảnh (`w:shd`), `(CONT'D)` theo cài đặt. Bộ cập nhật chấp nhận thêm tên repo `jenny-script` để sau này đổi tên không làm hỏng tự cập nhật (bản `jenny.3`–`jenny.5` sẽ không tự cập nhật được sau khi đổi tên).
- 06/11/2026 — Phát hành `v0.8.4-jenny.5` theo yêu cầu của Kha (sửa lỗi thiếu access token sau khi mở lại app, thêm chọn âm thanh gõ phím). Vẫn cần test tay: đẩy/kéo DOCX trên Google thật, Doc Hồ sơ, thông báo Doc đổi, âm thanh trong app, tự cập nhật từ `jenny.3`/`jenny.4`.
- 05/11/2026 — Thêm chọn âm thanh gõ phím: Máy đánh chữ (gốc), Bàn phím cơ, Vui nhộn, Piano, Trống, Violin. Âm thanh tự tổng hợp bằng `build/sounds/generate_typing_sounds.py` (không có bản quyền bên thứ ba), chưa ai nghe thử.
- 05/11/2026 — Sửa lỗi Kha gặp khi đẩy lên Docs: "Method doesn't allow unregistered callers". Mở lại app chỉ khôi phục refresh token, chưa có access token, request đầu đi không có token và Google trả 403 (không phải 401) nên client không refresh. Lỗi có từ Phase 1. Client giờ refresh trước khi gửi khi chưa có token.
- 05/11/2026 — Phát hành `v0.8.4-jenny.4` theo yêu cầu của Kha khi chưa thử tay; release notes ghi rõ phần Google Docs mới chưa thử trên Google thật. Cần test tay: đẩy/kéo DOCX, Doc Hồ sơ, thông báo Doc đổi, nút trên thanh công cụ, tự cập nhật từ `jenny.3`.
- 05/11/2026 — Đẩy lên Google Docs chuyển sang DOCX có dàn trang (Kha chọn hướng B); kéo về dùng `ScreenplayGoogleDocImporter`: dòng không đổi giữ loại theo kịch bản trong app, dòng mới nhận theo lề và nội dung (`NGOẠI.`, `CẢNH 1:`); Doc cũ dạng Fountain vẫn kéo về kiểu cũ. Thêm nút Google Docs trên thanh công cụ (theo góp ý không dùng chuột phải), thông báo khi Doc đổi (kiểm 1 phút/lần, chỉ khi app đang mở và đã đăng nhập). Trang bìa, logline, synopsis, treatment đẩy vào Doc "Hồ sơ" riêng trong cùng thư mục, chỉ một chiều, ghi đè phải hỏi nếu Doc đã bị sửa. Chưa thử trên Google thật.
- 05/11/2026 — Phát hành `v0.8.4-jenny.3` (Windows có tự cập nhật, thêm dmg macOS). Kha yêu cầu đăng khi chưa thử tay, release notes ghi rõ: chưa test tay tự cập nhật lẫn bản Mac. CI: dmg mở được đến màn hình chào trên Mac chip M khi đã giấu Qt (78 binary đủ 2 kiến trúc, tối thiểu macOS 13). Trang cineschool.org/jenny-script.html vẫn ghi chỉ có Windows.
- 04/11/2026 — Thêm job macOS vào `build_fork.yml` (dmg universal, chỉ ký ad-hoc, `build/mac/make_fork_dmg.sh`): kiểm kiến trúc và thư viện trong gói, mở app từ dmg khi đã giấu Qt, chụp màn hình. Không ai trong nhóm có Mac nên chưa có test tay; bản Mac không tự cập nhật.
- 04/11/2026 — Thêm kiểm tra và cài cập nhật lúc mở app (`AppUpdateManager`, +test; script cài đặt đã chạy thử 18 kiểm tra trên thư mục giả, gồm khôi phục bản cũ khi lỗi) — Kha test tay bằng `JENNY_SCRIPT_TEST_VERSION` trước khi đăng Facebook. Số phiên bản riêng nằm ở `update_info.h`, không đổi `applicationVersion` vì `database.cpp` so sánh nó.
- 04/11/2026 — Ẩn Trợ lý AI (cần cloud riêng của upstream, các kết nối nằm trong `#ifdef CLOUD_SERVICE_MANAGER`) ở onboarding, Settings và toolbar; bỏ câu danh ngôn của Joss Whedon; phát hành `v0.8.4-jenny.2` — Kha test tay. Muốn có AI thì phải tự làm kết nối riêng (mỗi người tự nhập API key).
- 04/11/2026 — Phát hành public cho bạn bè (`v0.8.4-jenny.1`), giữ tên Story Architect theo quyết định của Kha nên 0.5 rebrand vẫn để ngỏ; thêm `NOTICE.txt`, giấy phép trong zip, ghi nguồn ở màn hình Giới thiệu. OAuth đã chuyển In production (External), trang chủ và chính sách quyền riêng tư đặt ở `cineschool.org/jenny-script.html` và `/jenny-script-privacy.html` (bản gốc: `docs/website/`, đổi nội dung thì phải chép sang `public/` của web CineSchool rồi deploy); chưa thử đăng nhập bằng Gmail ngoài danh sách test.
- 04/11/2026 — Dán văn bản không có dòng trống (mỗi đoạn một dòng) giờ được nhận dạng từng dòng (`FountainTextNormalizer::formatPlainText`, +test); thêm lệnh "Tự động định dạng" ở menu chuột phải cho đoạn đã có trong editor — Kha test tay (nhất là Ctrl+Z sau khi định dạng cả kịch bản).
- 04/11/2026 — Thêm tự cuộn bằng nút giữa chuột kiểu trình duyệt (`MiddleButtonAutoScroll`, +1 test): đây mới là điều Kha yêu cầu, editor trước đó không xử lý nút giữa — Kha test tay.
- 04/11/2026 — Sửa thanh cuộn không kéo được (lỗi có thật nhưng không phải điều Kha hỏi, do hiểu nhầm yêu cầu): `ScrollBar` (upstream) tính vị trí theo kích thước cũ nên lệch ~90px vào trong và nhảy ra mép khi phình to dưới chuột; giờ resize trước rồi move. Thêm `tests/scrollbar_drag` (giả lập kéo, chạy trên CI). Dải xám tiêu đề cảnh bắt đầu đúng từ chữ đầu — Kha test tay thanh cuộn + checklist Google Docs; cân nhắc báo lỗi thanh cuộn cho upstream (hỏi trước).
- 04/11/2026 — Phase 1: GoogleDriveClient (+25 test), OAuth PKCE + QtKeychain 0.17.0, GoogleDocsManager (push/pull/open, kiểm sửa trên Docs bằng hash), menu chuột phải, đăng nhập ở onboarding và thẻ Tài khoản Google trong Settings, xử lý thiếu quyền Drive; Fountain importer sửa số cảnh / `..` / NỘI-NGOẠI (+17 test); tiếng Việt mặc định + sửa lỗi lưu ngôn ngữ onboarding; ẩn Thống kê; màu revision chuẩn dạng highlight; Courier New mặc định (Courier Prime thiếu 92/134 chữ Việt); dải xám dưới tiêu đề cảnh. Lỗi gặp: 403 access_denied (thiếu test user), thiếu scope Drive (bỏ tick ở consent) — Kha test tay checklist Google Docs (comment có mất khi ghi đè không), rồi gộp branch, làm nốt 0.5 rebrand và 0.9.
- 03/11/2026 — Phase 4 chen trước: `translation_vi.ts` (4.129/4.129 chuỗi; thuật ngữ dạng "Việt (English)"; INT./EXT., V.O., CONT'D giữ nguyên; giờ cảnh NGÀY/ĐÊM/SÁNG/TỐI), thêm Tiếng Việt vào danh sách ngôn ngữ, CI chạy lrelease — build CI, Kha test giao diện, team rà thuật ngữ; sau đó quay lại 0.5 và 0.9.
- 03/11/2026 — CI run 41627729069: Windows x64 build và đóng gói OK; Linux compile OK, test 41/42 (`continuous-offscreen-cursor` lệch 1px, đưa vào BLACKLIST); tắt telemetry — push commit mới, Kha smoke test bản zip, làm 0.5 rebrand và 0.9.
- 03/11/2026 — chốt D1–D6; fork lên `vienglacial/starc`, clone, init 2 submodule public; thêm `no_crashpad` (0.7); viết `build_fork.yml` (Linux: build và chạy test; Windows: zip portable) và `README_FORK.md` — push branch, bật Actions, đọc kết quả CI; tiếp theo là 0.5 rebrand, 0.6 endpoint (lưu ý `telemetry.storyapps.dev` gửi thống kê lúc khởi động).
