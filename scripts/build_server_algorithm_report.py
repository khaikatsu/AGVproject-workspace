"""Tạo báo cáo Word và hình minh họa từ mã/tài liệu AGV hiện tại."""

from __future__ import annotations

import json
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont
from docx import Document
from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Inches, Pt, RGBColor


ROOT = Path(__file__).resolve().parents[1]
FIG = ROOT / "docs" / "figures" / "server-report"
OUT = ROOT / "docs" / "exports" / "Bao_cao_thuat_toan_server_AGV.docx"
FIG.mkdir(parents=True, exist_ok=True)
OUT.parent.mkdir(parents=True, exist_ok=True)

NAVY = "#193454"
TEAL = "#127F7A"
BLUE = "#3B68A5"
AMBER = "#D58A2D"
RED = "#AC4650"
PALE = "#F3F6F9"
GRID = "#D8E0E8"
DARK = "#202B35"
MUTED = "#596B7A"
WHITE = "#FFFFFF"
FONT = Path("C:/Windows/Fonts/arial.ttf")
FONT_BOLD = Path("C:/Windows/Fonts/arialbd.ttf")


def f(size: int, bold: bool = False):
    return ImageFont.truetype(str(FONT_BOLD if bold else FONT), size)


def canvas(width: int, height: int):
    im = Image.new("RGB", (width, height), WHITE)
    d = ImageDraw.Draw(im)
    return im, d


def text_lines(d, xy, value, width, size=28, color=DARK, bold=False, align="center", gap=6):
    font = f(size, bold)
    words = value.split()
    lines, current = [], ""
    for word in words:
        test = f"{current} {word}".strip()
        if current and d.textbbox((0, 0), test, font=font)[2] > width:
            lines.append(current)
            current = word
        else:
            current = test
    if current:
        lines.append(current)
    x, y = xy
    step = size + gap
    for line in lines:
        box = d.textbbox((0, 0), line, font=font)
        lw = box[2] - box[0]
        xx = x - lw / 2 if align == "center" else x
        d.text((xx, y), line, font=font, fill=color)
        y += step
    return y


def box(d, rect, title, subtitle="", fill=PALE, border=GRID, title_color=DARK, title_size=31, sub_size=23):
    x0, y0, x1, y1 = rect
    d.rounded_rectangle(rect, radius=20, fill=fill, outline=border, width=3)
    if subtitle:
        text_lines(d, ((x0+x1)/2, y0+24), title, x1-x0-32, title_size, title_color, True)
        text_lines(d, ((x0+x1)/2, y0+75), subtitle, x1-x0-35, sub_size, MUTED)
    else:
        lines = len(title.split()) // 4 + 1
        top = (y0+y1)/2 - (lines * (title_size+4))/2
        text_lines(d, ((x0+x1)/2, top), title, x1-x0-35, title_size, title_color, True)


def arrow(d, start, end, color=NAVY, width=5, head=16):
    x0, y0 = start
    x1, y1 = end
    d.line((x0, y0, x1, y1), fill=color, width=width)
    angle = math.atan2(y1-y0, x1-x0)
    p1 = (x1 - head*math.cos(angle-0.48), y1 - head*math.sin(angle-0.48))
    p2 = (x1 - head*math.cos(angle+0.48), y1 - head*math.sin(angle+0.48))
    d.polygon([(x1, y1), p1, p2], fill=color)


def title(d, value):
    d.text((65, 35), value, font=f(37, True), fill=NAVY)
    d.line((65, 93, 1735, 93), fill=GRID, width=3)


def architecture():
    im, d = canvas(1800, 760)
    title(d, "Kiến trúc điều phối hai robot AGV")
    y0, y1 = 180, 415
    blocks = [
        (85, 330, "Trình duyệt", "Bản đồ, nhiệm vụ, pin, lỗi"),
        (415, 700, "Gateway Node.js", "HTTP API và WS trình duyệt"),
        (800, 1100, "Core C++", "Map, Fleet, Mutex, Shift"),
        (1200, 1450, "MQTT broker", "Pub/sub theo robot"),
        (1530, 1760, "Hai robot", "ESP32, RFID, cảm biến"),
    ]
    for x0, x1, a, b in blocks:
        box(d, (x0, y0, x1, y1), a, b, fill="#F0F5FA", border="#BFCFDE")
    for (x0, x1, label) in [(330,415,"HTTP / WS"),(700,800,"WS v1"),(1100,1200,"MQTT"),(1450,1530,"MQTT / WiFi")]:
        arrow(d, (x0+10, 280), (x1-10, 280), TEAL, 5)
        arrow(d, (x1-10, 318), (x0+10, 318), BLUE, 4)
        text_lines(d, ((x0+x1)/2, 230), label, x1-x0+55, 18, DARK, True)
    box(d, (440, 515, 675, 650), "SQLite", "Lịch sử, KPI, snapshot", fill="#FFF9EF", border="#E4CB9E", title_size=27, sub_size=21)
    arrow(d, (555, 415), (555, 505), AMBER, 5)
    arrow(d, (595, 505), (595, 415), AMBER, 4)
    box(d, (825, 515, 1075, 650), "MapGraph", "Dijkstra và trọng số", fill="#EFF7F5", border="#B7D5D1", title_size=27, sub_size=21)
    arrow(d, (950, 415), (950, 505), TEAL, 5)
    d.text((90, 694), "Nguồn hình: tổng hợp từ AGVserver.cpp, gateway.js và hợp đồng web/server trong workspace.", font=f(22), fill=MUTED)
    im.save(FIG / "01-kien-truc.png")


def server_loops():
    im, d = canvas(1800, 1020)
    title(d, "Ba luồng làm thay đổi trạng thái Core")
    lanes = [
        (140, "Lệnh từ web", BLUE, ["Gateway kiểm tra", "Core xử lý lệnh", "Checkpoint rồi trả result"]),
        (375, "Bản tin MQTT", TEAL, ["Kiểm tra topic", "Cập nhật Fleet / Mutex", "Checkpoint rồi publish"]),
        (610, "Vòng lặp chính", NAVY, ["fleet.tick và shift.tick", "Checkpoint rồi publish", "Broadcast state"]),
    ]
    for top, label, c, steps in lanes:
        d.rounded_rectangle((65, top, 1735, top+170), radius=20, fill="#F7F9FB", outline=GRID, width=3)
        d.rounded_rectangle((78, top+16, 330, top+154), radius=14, fill=c)
        text_lines(d, (204, top+62), label, 215, 30, WHITE, True)
        xs = [410, 805, 1200]
        for i, step in enumerate(steps):
            box(d, (xs[i], top+33, xs[i]+310, top+138), step, fill=WHITE, border="#C7D5E2", title_size=30)
            if i < 2:
                arrow(d, (xs[i]+315, top+85), (xs[i+1]-10, top+85), c, 4)
    box(d, (420, 855, 1380, 963), "Quy tắc chung: lưu checkpoint trước command hoặc grant", fill="#FFF9EF", border="#E5CDAA", title_size=30)
    im.save(FIG / "02-ba-luong-server.png")


def decision():
    im, d = canvas(1800, 1000)
    title(d, "Quyết định giao nhiệm vụ trong Core")
    main = [
        (145, "Nhận task_id và priority"),
        (275, "Kiểm tra ca, MQTT, vị trí, lỗi và pin"),
        (405, "Lọc AGV rảnh và có thể đi"),
        (535, "Dijkstra theo tổng trọng số, né vùng bận"),
        (665, "Chọn xe và route; hòa điểm chọn ID ổn định"),
        (795, "Checkpoint rồi gửi GOTO có cmd_id"),
    ]
    for i, (yy, label) in enumerate(main):
        box(d, (385, yy, 1395, yy+88), label, fill="#F0F5FA", border="#B9CCDD", title_size=31)
        if i < len(main)-1:
            arrow(d, (890, yy+92), (890, main[i+1][0]-8), BLUE, 4)
    box(d, (90, 330, 315, 610), "Không đủ điều kiện", "Giữ pending hoặc từ chối lệnh mới", fill="#FFF8EC", border="#E7D0A8", title_size=25, sub_size=21)
    arrow(d, (385, 450), (323, 450), AMBER, 4)
    box(d, (1480, 460, 1730, 730), "Không có đường", "Giữ chờ, thử lại sau khi trạng thái đổi", fill="#FFF8EC", border="#E7D0A8", title_size=25, sub_size=21)
    arrow(d, (1395, 570), (1470, 570), AMBER, 4)
    d.text((65, 935), "ACK xác nhận nhận lệnh; nhiệm vụ chỉ hoàn thành khi telemetry RFID hợp lệ xác nhận node đích.", font=f(24), fill=MUTED)
    im.save(FIG / "03-phan-cong-nhiem-vu.png")


def warehouse_map():
    m = json.loads((ROOT / "data" / "map_layout.json").read_text(encoding="utf-8"))
    im, d = canvas(1800, 1080)
    title(d, "Đồ thị kho v3 đang dùng trong Core")
    pts = {}
    for n in m["nodes"]:
        x = 180 + (n["x"] + 20) / 240 * 1450
        y = 170 + (n["y"] + 20) / 180 * 735
        pts[n["id"]] = (x, y)
    # Mỗi cạnh được vẽ theo chiều trong JSON; hai chiều hiện thành hai mũi tên lệch nhẹ.
    for e in m["edges"]:
        a, b = pts[e["from"]], pts[e["to"]]
        vx, vy = b[0]-a[0], b[1]-a[1]
        length = max(1, math.hypot(vx, vy))
        nx, ny = -vy/length*5, vx/length*5
        s = (a[0]+vx/length*22+nx, a[1]+vy/length*22+ny)
        t = (b[0]-vx/length*27+nx, b[1]-vy/length*27+ny)
        arrow(d, s, t, "#A3B3C2", 3, 11)
    for n in m["nodes"]:
        x, y = pts[n["id"]]
        typ = n["type"]
        c = TEAL if typ == "mutex" else AMBER if typ == "charging" else BLUE if typ == "station" else NAVY
        radius = 24 if typ in ("mutex", "charging") else 19
        d.ellipse((x-radius,y-radius,x+radius,y+radius), fill=WHITE, outline=c, width=6)
        d.text((x, y-10), n["id"], font=f(23, True), fill=c, anchor="mm")
    legend = [(TEAL,"Mutex M1–M6"),(AMBER,"Trạm sạc S1/S2"),(BLUE,"Kho K1–K6"),(NAVY,"Node nối tuyến")]
    for i, (c, label) in enumerate(legend):
        xx = 135 + i*420
        d.ellipse((xx, 955, xx+25, 980), fill=WHITE, outline=c, width=5)
        d.text((xx+36, 950), label, font=f(24), fill=DARK)
    d.text((65, 1015), "Nguồn dữ liệu hình: data/map_layout.json; tọa độ là tọa độ bản đồ, không phải kích thước vật lý đã đo.", font=f(21), fill=MUTED)
    im.save(FIG / "04-ban-do-kho.png")


def evidence_timeline():
    im, d = canvas(1800, 690)
    title(d, "Ba mốc cần phân biệt trong một nhiệm vụ")
    y = 345
    arrow(d, (160, y), (1640, y), BLUE, 7, 24)
    stages = [
        (250, "HTTP 200", "Core chấp nhận\nlệnh và cấp task_id", BLUE),
        (730, "ACK cmd_id", "Robot xác nhận\nđã nhận command", AMBER),
        (1210, "RFID đích", "Telemetry hợp lệ;\nCore ghi completed", TEAL),
    ]
    for xx, a, b, c in stages:
        d.ellipse((xx-22,y-22,xx+22,y+22),fill=WHITE,outline=c,width=7)
        d.rounded_rectangle((xx-175, 115, xx+175, 255),radius=19,fill="#F6F9FB",outline=GRID,width=3)
        text_lines(d,(xx,145),a,315,36,c,True)
        text_lines(d,(xx,195),b.replace("\n"," "),320,27,DARK)
        d.line((xx,255,xx,y-30),fill=c,width=4)
    d.rounded_rectangle((290,445,1510,585),radius=17,fill="#FFF8ED",outline="#E8CFA8",width=3)
    text_lines(d,(900,475),"Chỉ mốc RFID đích là bằng chứng phần mềm về đến node",1120,30,DARK,True)
    text_lines(d,(900,526),"Không mốc nào tự chứng minh robot đã dừng an toàn hoặc giao hàng vật lý",1120,24,MUTED)
    im.save(FIG / "05-ack-va-rfid.png")


def recovery():
    im, d = canvas(1800, 760)
    title(d, "Xử lý mất kết nối và khởi động lại")
    items = [
        (80, 180, 530, 355, "Mất MQTT hoặc telemetry", "Gián đoạn task; giữ zone/route chưa xác minh", RED),
        (670, 180, 1120, 355, "Core khởi động lại", "Nạp queue/mutex; vị trí cũ chưa được xác minh", AMBER),
        (1260, 180, 1720, 355, "Gateway mất Core", "Đánh dấu state cũ; lệnh chờ có kết quả unknown", BLUE),
    ]
    for x0,y0,x1,y1,a,b,c in items:
        box(d,(x0,y0,x1,y1),a,b,fill="#F9FAFC",border=c,title_color=c,title_size=26,sub_size=22)
    for xx in (305,895,1490):
        arrow(d,(xx,365),(xx,465),NAVY,5)
    box(d,(390,475,1400,630),"Cần telemetry RFID mới và đối soát người vận hành", "Không suy luận đường đã trống từ ACK, mất tín hiệu hoặc restart",fill="#EFF7F5",border="#A9D2C9",title_size=30,sub_size=24)
    im.save(FIG / "06-phuc-hoi.png")


def set_repeat_header(row):
    trPr = row._tr.get_or_add_trPr()
    e = OxmlElement("w:tblHeader")
    e.set(qn("w:val"), "true")
    trPr.append(e)


def set_borders(table):
    tblPr = table._tbl.tblPr
    borders = OxmlElement("w:tblBorders")
    for edge in ("top", "left", "bottom", "right", "insideH", "insideV"):
        el = OxmlElement("w:" + edge)
        el.set(qn("w:val"), "single")
        el.set(qn("w:sz"), "5")
        el.set(qn("w:color"), "D9D9D9")
        borders.append(el)
    tblPr.append(borders)


def shade(cell, color):
    tcPr = cell._tc.get_or_add_tcPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:fill"), color)
    tcPr.append(shd)


def cell_margin(cell, top=95, start=115, bottom=95, end=115):
    tc = cell._tc
    tcPr = tc.get_or_add_tcPr()
    mar = tcPr.first_child_found_in("w:tcMar")
    if mar is None:
        mar = OxmlElement("w:tcMar")
        tcPr.append(mar)
    for side, val in (("top",top),("start",start),("bottom",bottom),("end",end)):
        tag = "w:" + side
        node = mar.find(qn(tag))
        if node is None:
            node = OxmlElement(tag)
            mar.append(node)
        node.set(qn("w:w"),str(val))
        node.set(qn("w:type"),"dxa")


def add_table(doc, headers, rows, widths=None):
    table = doc.add_table(rows=1, cols=len(headers))
    table.autofit = False
    if widths:
        tblW=table._tbl.tblPr.find(qn("w:tblW"))
        if tblW is None:
            tblW=OxmlElement("w:tblW")
            table._tbl.tblPr.append(tblW)
        tblW.set(qn("w:w"),str(int(sum(widths)*567)))
        tblW.set(qn("w:type"),"dxa")
        for i,w in enumerate(widths):
            table.columns[i].width = Cm(w)
    for i,h in enumerate(headers):
        cell = table.rows[0].cells[i]
        if widths: cell.width=Cm(widths[i])
        cell.text = h
        shade(cell,"24415D")
        cell_margin(cell)
        cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
        for p in cell.paragraphs:
            p.paragraph_format.space_after = Pt(0)
            for run in p.runs:
                run.bold = True
                run.font.color.rgb = RGBColor(255,255,255)
                run.font.size = Pt(9.5)
    set_repeat_header(table.rows[0])
    for idx, row in enumerate(rows):
        cells=table.add_row().cells
        for i,value in enumerate(row):
            cells[i].text=str(value)
            if widths: cells[i].width=Cm(widths[i])
            if idx%2: shade(cells[i],"F1F5F8")
            cell_margin(cells[i])
            cells[i].vertical_alignment=WD_CELL_VERTICAL_ALIGNMENT.CENTER
            for p in cells[i].paragraphs:
                p.paragraph_format.space_after = Pt(0)
                p.paragraph_format.line_spacing = 1.05
                for run in p.runs:
                    run.font.size = Pt(9.2)
    set_borders(table)
    doc.add_paragraph().paragraph_format.space_after=Pt(0)
    return table


def add_para(doc, text, bold_lead=None):
    p=doc.add_paragraph()
    if bold_lead and text.startswith(bold_lead):
        p.add_run(bold_lead).bold=True
        p.add_run(text[len(bold_lead):])
    else:
        p.add_run(text)
    return p


def add_bullet(doc, text):
    p=doc.add_paragraph(style="List Bullet")
    p.add_run(text)
    return p


def add_figure(doc, filename, caption, width=16.3):
    p=doc.add_paragraph()
    p.alignment=WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_before=Pt(8)
    p.paragraph_format.space_after=Pt(0)
    p.paragraph_format.keep_with_next=True
    p.add_run().add_picture(str(FIG/filename),width=Cm(width))
    c=doc.add_paragraph(style="Caption")
    c.alignment=WD_ALIGN_PARAGRAPH.CENTER
    c.add_run(caption)


def add_page_num(p):
    p.add_run("Trang ")
    field = OxmlElement("w:fldSimple")
    field.set(qn("w:instr"),"PAGE")
    p._p.append(field)


def build_doc():
    doc=Document()
    sec=doc.sections[0]
    sec.page_height=Cm(29.7)
    sec.page_width=Cm(21)
    sec.top_margin=Cm(2.0)
    sec.bottom_margin=Cm(1.8)
    sec.left_margin=Cm(2.1)
    sec.right_margin=Cm(2.1)
    sec.header_distance=Cm(0.9)
    sec.footer_distance=Cm(0.8)

    styles=doc.styles
    normal=styles["Normal"]
    normal.font.name="Arial"; normal.font.size=Pt(10.5); normal.font.color.rgb=RGBColor(32,43,53)
    normal.paragraph_format.line_spacing=1.18
    normal.paragraph_format.space_after=Pt(7)
    for name,size,before,after in (("Title",21,0,14),("Heading 1",15,17,8),("Heading 2",12,12,5)):
        s=styles[name]
        s.font.name="Arial";s.font.size=Pt(size);s.font.bold=True;s.font.color.rgb=RGBColor(0,0,0)
        s.paragraph_format.space_before=Pt(before)
        s.paragraph_format.space_after=Pt(after)
        s.paragraph_format.keep_with_next=True
    title_ppr=styles["Title"]._element.get_or_add_pPr()
    title_border=title_ppr.find(qn("w:pBdr"))
    if title_border is not None: title_ppr.remove(title_border)
    styles["Subtitle"].font.name="Arial";styles["Subtitle"].font.size=Pt(11)
    styles["Subtitle"].font.color.rgb=RGBColor(0,0,0)
    styles["Caption"].font.name="Arial";styles["Caption"].font.size=Pt(9)
    styles["Caption"].font.italic=True;styles["Caption"].font.color.rgb=RGBColor(65,76,88)
    styles["Caption"].paragraph_format.space_after=Pt(11)
    styles["List Bullet"].font.name="Arial";styles["List Bullet"].font.size=Pt(10.5)
    styles["List Bullet"].paragraph_format.space_after=Pt(4)

    head=sec.header.paragraphs[0]
    head.alignment=WD_ALIGN_PARAGRAPH.RIGHT
    head.add_run("BÁO CÁO KỸ THUẬT  |  HỆ THỐNG AGV").font.size=Pt(8)
    foot=sec.footer.paragraphs[0]
    foot.alignment=WD_ALIGN_PARAGRAPH.RIGHT
    add_page_num(foot)

    p=doc.add_paragraph(style="Title")
    p.add_run("Thuật toán và kiến trúc server điều phối hai robot AGV trong kho")
    p=doc.add_paragraph(style="Subtitle")
    p.add_run("Báo cáo phân tích từ mã nguồn và lưu đồ hệ thống")
    add_para(doc,"Ngày đối chiếu: 07/10/2026  |  Phạm vi: web, gateway, Core, MQTT và lưu trữ")
    add_para(doc,"Báo cáo giải thích cách server nhận nhiệm vụ, chọn đường và điều phối hai xe, sau đó liên hệ từng lựa chọn kỹ thuật với mục tiêu đề tài. Những khẳng định về hoạt động của phần mềm được đối chiếu với mã trong workspace; tài liệu không xem ACK hoặc bản mô phỏng là bằng chứng robot thật đã di chuyển an toàn.")
    add_figure(doc,"01-kien-truc.png","Hình 1. Các thành phần và kênh trao đổi chính trong hệ thống.")

    doc.add_heading("1 Mục tiêu và phạm vi",level=1)
    add_para(doc,"Đề tài đặt ra bốn mục tiêu nguồn: hai robot đi theo lộ trình định trước, web giám sát gần thời gian thực, quản lý bản đồ/lộ trình/pin/lỗi và có dữ liệu phục vụ báo cáo nghiên cứu [P1, P2]. Server được hiểu là cặp gateway Node.js và Core C++; ESP32 là bộ điều khiển trên robot, không nằm trong vòng điều khiển motor của server.")
    add_para(doc,"Căn cứ phân tích là AGVserver.cpp, FLeetmanager.hpp, MapGraph.hpp, Mutexarbiter.hpp, ShiftController.hpp, gateway.js, hợp đồng mạng và sổ yêu cầu hiện tại [P1–P7]. Trạng thái SOURCE chỉ mục tiêu người dùng nêu; OBSERVED chỉ hành vi đọc được từ mã; OPEN chỉ nội dung cần quyết định hoặc thử nghiệm tiếp. Các số 200 ms, 3 s và 10 s dưới đây là cấu hình trong mã hiện tại, chưa phải chỉ tiêu nghiệm thu.")

    doc.add_heading("2 Các khái niệm cần phân biệt",level=1)
    add_table(doc,["Thuật ngữ","Định nghĩa trong báo cáo"],[
        ("AGV","Robot tự hành nhận lệnh di chuyển theo node RFID, tự đọc cảm biến và điều khiển motor tại firmware."),
        ("Gateway","Dịch vụ Express cung cấp HTTP/WS cho web, chuyển lệnh đến Core và lưu SQLite."),
        ("Core","Tiến trình C++ sở hữu bản đồ đang chạy, đội xe, nhiệm vụ và quyền vùng chung."),
        ("Task và task_id","Yêu cầu đi đến node; task_id giúp theo dõi một nhiệm vụ qua pending, dispatched, completed, cancelled hoặc interrupted."),
        ("Telemetry","Bản tin robot gửi về gồm node RFID, pin và có thể có cờ vật cản. Bản hiện tại dùng chuỗi phân cách dấu |."),
        ("ACK","Biên nhận đúng cmd_id hoặc grant_id; chưa chứng minh xe đã hoàn tất chuyển động hay rời vùng."),
        ("Mutex zone","Vùng đường dùng chung chỉ cấp quyền logic cho một robot tại một thời điểm; owner, hàng chờ và grant_id do Core quản lý."),
        ("Checkpoint","Ảnh trạng thái bền vững của queue/fleet/mutex được lưu trước khi Core phát command hoặc grant."),
    ],[4.2,12.1])
    add_para(doc,"HTTP phù hợp với thao tác và truy vấn có phản hồi; WebSocket cho luồng trạng thái hai chiều đang mở [E2, E3]; MQTT dùng publish/subscribe giữa Core và thiết bị robot [E1]. Dùng ba kênh theo vai trò giảm việc suy diễn sai: HTTP thành công là Core chấp nhận yêu cầu, còn hoàn thành chuyển động cần telemetry RFID đích.")

    doc.add_heading("3 Kiến trúc và chủ sở hữu trạng thái",level=1)
    add_para(doc,"Trình duyệt gửi lệnh qua gateway cổng 3000. Gateway xác thực và kiểm tra dữ liệu, tạo request_id rồi trao đổi với Core qua WebSocket cổng 8080. Core kết nối broker MQTT; robot dùng topic agv/{id}/{action}. SQLite nằm ở gateway để lưu lịch sử, snapshot bản đồ và KPI. Core là nguồn quyết định cho graph đang chạy, task và mutex; snapshot SQLite không tự áp dụng lại khi reconnect [P3, P4].")
    add_para(doc,"Sự phân quyền này phù hợp với kho có hai xe: cùng một trạng thái điều phối phải được đọc nhất quán khi web gửi lệnh, robot báo RFID và nhịp kiểm tra timeout xảy ra gần nhau. g_stateMtx bảo vệ dữ liệu dùng chung giữa các luồng trong Core. Nó khác mutex zone, vốn là quyền đi qua giao lộ của hai thiết bị vật lý và phải truyền qua MQTT [P3, P5].")
    doc.add_heading("3.1 Chủ sở hữu dữ liệu",level=2)
    add_table(doc,["Thành phần","Dữ liệu chịu trách nhiệm"],[
        ("Robot và firmware","RFID/cảm biến/pin đo được, trạng thái motor và biên nhận lệnh."),
        ("Core C++","Bản đồ đang chạy, queue, task, đường dự kiến, quyền vùng và kết quả điều phối."),
        ("Gateway và SQLite","Lịch sử đã nhận, KPI, snapshot lưu theo thao tác và trạng thái liên kết."),
        ("Dashboard","Hiển thị trạng thái Core cùng tuổi dữ liệu; gửi yêu cầu người vận hành."),
    ],[4.2,12.1])

    doc.add_heading("4 Luồng hoạt động của server",level=1)
    add_figure(doc,"02-ba-luong-server.png","Hình 2. Ba nhánh sự kiện cùng tác động đến trạng thái Core; hình giản lược thứ tự hàm.")
    doc.add_heading("4.1 Khởi động và khôi phục",level=2)
    add_para(doc,"Core giữ cổng WS trước khi dùng counter và MQTT để ngăn hai Core cùng điều khiển một đội xe. Tiếp theo, Core nạp bản đồ, bộ đếm cmd_id/task_id, runtime_state và trạng thái ca. Nếu checkpoint không khớp map, tiến trình dừng để đối soát. Queue và owner mutex có thể được khôi phục, nhưng vị trí cũ của robot không được xem là vị trí mới; cần RFID/telemetry mới [P3, P6].")
    doc.add_heading("4.2 Lệnh từ người vận hành",level=2)
    add_para(doc,"Gateway nhận REST, kiểm tra quyền, kiểu dữ liệu, trạng thái Core và MQTT rồi gửi lệnh có request_id. WebCommands thực hiện thêm/hủy task, GOTO chỉ định xe, sửa bản đồ, quản lý vật cản, ca và xác minh vùng. Core trả command_result kèm core_session_id và state. Nếu liên kết đứt trong lúc chờ, gateway trả unknown để người vận hành đối soát, không tự phát lại lệnh có thể đã được Core xử lý [P3, P4].")
    doc.add_heading("4.3 Sự kiện MQTT từ robot",level=2)
    add_para(doc,"Callback on_message kiểm tra topic, ID, độ dài tối đa 4096 byte, retain và cú pháp payload. Từng topic chuyển đến FleetManager hoặc MutexArbiter: telemetry cập nhật RFID/pin/vật cản; ack xác nhận command; mutex_request, mutex_ack và mutex_release điều phối vùng; error/status/offline/charging_done thay đổi trạng thái vận hành [P3, P7]. Bản tin không hợp lệ bị bỏ qua hoặc ghi lỗi; sự kiện vật cản/lỗi có thể gọi requestWake để vòng điều phối chạy sớm.")
    doc.add_heading("4.4 Nhịp điều phối và phát trạng thái",level=2)
    add_para(doc,"Trong mỗi vòng, fleet.tick() xử lý timeout telemetry, retry ACK, tuyến BLOCKED, phân công queue và pin. Khi MQTT kết nối, shift.tick() thực hiện logic ca; Core thử lại grant chưa ACK và gửi heartbeat. checkpointAndPublish() lưu trước khi phát bản tin xếp hàng. Sau đó broadcastState() gửi snapshot/delta tối đa 5 lần mỗi giây. Core nhả g_stateMtx trước khi chờ tối đa 200 ms hoặc được requestWake đánh thức [P3].")

    doc.add_heading("5 Thuật toán tìm đường và phân công",level=1)
    add_figure(doc,"03-phan-cong-nhiem-vu.png","Hình 3. Các điều kiện và quyết định khi Core phân công một nhiệm vụ.")
    doc.add_heading("5.1 Mô hình đồ thị và Dijkstra",level=2)
    add_para(doc,"Bản đồ được biểu diễn bằng G = (V, E): V là node như RFID, kho và trạm; E là cạnh có hướng, trọng số không âm. MapGraph tìm đường bằng Dijkstra và so tổng trọng số, không so số node. Với dữ liệu bản đồ hiện tại, đây là cách chọn đường có chi phí nhỏ nhất theo trọng số đã nhập, miễn là các cạnh hợp lệ và không bị chặn [P5, E4]. Tọa độ x/y của hình bản đồ chỉ giúp hiển thị, chưa phải phép đo kích thước vật lý.")
    add_figure(doc,"04-ban-do-kho.png","Hình 4. Đồ thị 26 node và 39 cạnh từ data/map_layout.json, gồm sáu mutex zone M1–M6.")
    add_para(doc,"Dijkstra phù hợp vì kho có đồ thị nhỏ, rõ hướng và trọng số; thuật toán không cần giả định một heuristic từ tọa độ pixel. Khi có vật cản hoặc đường/zone chưa xác minh, Core loại các node liên quan khỏi tập ứng viên. Kết quả chỉ là đường trên đồ thị, không tự bảo đảm khoảng cách phanh, độ rộng lối đi hay xe quay không quét vào vùng xe khác.")
    doc.add_heading("5.2 Chọn xe và giữ tuyến",level=2)
    add_para(doc,"Task chờ được xét theo priority rồi thứ tự tạo. Core lọc xe rảnh, online, có RFID hợp lệ, không lỗi, có pin đủ và ca cho phép nhận nhiệm vụ. Với mỗi xe phù hợp, Core tính tuyến và tổng trọng số tới đích; nếu bằng nhau dùng ID robot để chọn ổn định. Các node/route đang dùng và vùng chưa xác minh được đưa vào tập cản; task chưa có đường hoặc chưa có xe giữ ở queue để xét lại [P5].")
    add_para(doc,"Cách này phù hợp mục tiêu có hai AGV vì giảm trường hợp cả hai được giao cùng một đoạn đường ngay trong khâu lập tuyến. Nó không phải tối ưu lịch toàn đội; hai task có thể phải chờ dù một lịch phối hợp khác tốt hơn. Chế độ thử riêng xe 01 có điều kiện khác và không dùng bộ phân công queue hai xe [P5, P6].")
    doc.add_heading("5.3 Quyền vùng giao nhau",level=2)
    add_para(doc,"MutexArbiter lưu owner và hàng chờ FIFO cho từng zone. Robot phải request trước khi vào, nhận grant có grant_id, ACK grant và chỉ release bằng đúng token sau khi toàn thân rời vùng. Thiếu ACK grant dẫn đến retry cùng ID, không nhường quyền cho xe khác. Khi robot offline, Core giữ owner để tránh suy luận sai rằng giao lộ đã trống [P3, P7].")
    add_para(doc,"Đây là lựa chọn phù hợp với hai robot độc lập qua mạng: std::mutex trong Core chỉ bảo vệ bộ nhớ, còn grant là thông điệp quyền đi mà firmware có thể kiểm tra. Mức an toàn vật lý vẫn phụ thuộc vị trí RFID, hình học xe, khoảng dừng và cách firmware xác nhận đã rời hẳn vùng; những điểm này chưa được chứng minh trên hai xe thật.")
    doc.add_heading("5.4 Pin và ca làm việc",level=2)
    add_para(doc,"Core theo dõi pin thấp để chuyển nhu cầu sạc và dùng hai ngưỡng thấp/cho phép nhận lại nhằm tránh dao động giao việc quanh một giá trị. ShiftController quản lý mở ca, đóng ca và trạm riêng 01→S1, 02→S2. Kết thúc ca giữ task đang chạy tới khi hoàn thành, dừng nhận task mới rồi đưa xe về trạm theo telemetry, không gán vị trí giả [P6]. Khi pin chưa được hiệu chuẩn, giá trị ? được hiển thị là chưa đo; chính sách thử một xe có ngoại lệ riêng.")

    doc.add_heading("6 Bằng chứng hoàn thành và dữ liệu lưu trữ",level=1)
    add_figure(doc,"05-ack-va-rfid.png","Hình 5. HTTP, ACK và RFID trả lời ba câu hỏi khác nhau trong vòng đời task.")
    add_para(doc,"HTTP 200 nói Core đã nhận và chấp nhận thao tác; ACK cmd_id nói robot đã nhận command; telemetry RFID đến đích sau khi đi hết tuyến mới làm Core đặt task completed. Vì vậy giao diện và lịch sử dùng task_id/trạng thái Core, không đoán từ việc target biến mất hoặc một bản ACK [P4, P5].")
    add_para(doc,"Gateway lưu thay đổi task, vị trí, pin và log vào SQLite; trạng thái mới từ Core được đẩy tới dashboard bằng snapshot và state_delta. Delta theo trường giảm lượng dữ liệu khi bản đồ không đổi. Gateway gắn thời gian nhận để báo dữ liệu cũ; nếu mất Core, nhãn UNKNOWN phản ánh khoảng quan sát thiếu, không chứng minh robot đã dừng. SQLite hỗ trợ transaction cho lịch sử trong cùng một cơ sở dữ liệu [P4, E5].")
    add_table(doc,["Dữ liệu","Nơi sở hữu","Ý nghĩa khi mất kết nối"],[
        ("Task đang chạy, queue, owner mutex","Core và runtime_state.json","Giữ để phục hồi/đối soát; task đang chạy thành interrupted sau restart."),
        ("cmd_id, task_id","Counter riêng của Core","Không rewind hoặc dùng chung giữa hai Core; tránh lặp định danh."),
        ("History, log, KPI, snapshot map","SQLite ở gateway","Phục vụ tra cứu và báo cáo; không thay bản đồ Core tự động."),
        ("RFID, pin, lỗi mới","Robot → MQTT → Core","Thiếu telemetry thì vị trí và an toàn vùng không được xác minh."),
    ],[4.0,4.0,8.3])

    doc.add_heading("7 Lý do kiến trúc phù hợp với đề tài",level=1)
    add_table(doc,["Mục tiêu nguồn","Cơ chế đáp ứng","Giới hạn cần nêu"],[
        ("Hai xe đi tuyến định trước","Đồ thị có hướng, Dijkstra, chọn xe, giữ route và mutex zone","Chưa đo tránh va chạm hình học trên hai xe thật."),
        ("Web giám sát gần thời gian thực","MQTT telemetry, Core snapshot/delta, WebSocket đến dashboard","Chưa có ngưỡng độ trễ nghiệm thu đã chốt."),
        ("Quản lý bản đồ, tuyến, pin, lỗi","WebCommands, FleetManager, ShiftController, trạng thái đo từ RFID","Pin/sạc/motor thật còn cần hiệu chuẩn và thử bàn."),
        ("Dữ liệu cho báo cáo nghiên cứu","Task history, log, KPI, map snapshot và kịch bản test","Chưa thay mẫu báo cáo chính thức của học viện."),
    ],[3.4,6.7,6.2])
    add_para(doc,"Các thành phần được chọn cũng khớp stack đang có: C++17 cho Core, Node.js/Express cho gateway, HTML/JS cho dashboard, libmosquitto và SQLite cho giao tiếp/lịch sử. Giữ stack hiện hành giúp đối chiếu trực tiếp giữa báo cáo, mã và ca kiểm thử, đồng thời tránh mô tả một hệ thống khác chưa được triển khai [P3, P4].")

    doc.add_heading("8 Sự cố, phục hồi và giới hạn kiểm chứng",level=1)
    add_figure(doc,"06-phuc-hoi.png","Hình 6. Phản ứng thận trọng khi dữ liệu hoặc liên kết điều khiển bị gián đoạn.")
    add_para(doc,"Mất telemetry hoặc broker không chứng minh xe rời vùng. Core gián đoạn task, giữ đường/zone chưa xác minh và yêu cầu đối soát trước khi tái giao. Sau restart, pending và quyền vùng có thể khôi phục từ checkpoint nhưng vị trí robot cần RFID mới; ca trở về trạng thái tập kết. Gateway mất Core đánh dấu dữ liệu cũ và kết quả lệnh đang chờ là unknown [P3, P4, P6].")
    add_para(doc,"MQTT hiện dùng QoS 0, tức cơ chế giao nhận ở mức giao thức là at most once [E1]. ACK ứng dụng, retry cùng cmd_id và chống lệnh trùng giúp xử lý mất/trùng trong một số kịch bản, nhưng không chứng minh motor đã chạy. Checkpoint trước publish ngăn phát lệnh dựa trên trạng thái chưa lưu; nếu ghi checkpoint thất bại, Core dừng phát lệnh [P3, P6].")
    add_para(doc,"Kết quả mô phỏng và kiểm thử phần mềm trong workspace hỗ trợ đánh giá logic, nhưng không thay phép đo trên phần cứng. Trước nghiệm thu hai robot cần kiểm tra bề rộng lối đi, kích thước và quỹ đạo quay, độ trễ RFID, phanh khi mất mạng, nguồn motor, pin và quá trình release mutex thực tế. Tiêu chí định lượng về độ trễ, số lượt chạy và sai lệch cho phép vẫn OPEN trong sổ yêu cầu [P1, P2].")

    doc.add_heading("9 Kết luận",level=1)
    add_para(doc,"Server hiện có một chuỗi quyết định nhất quán từ lệnh web đến nhiệm vụ, đường đi, quyền vùng, xác nhận RFID và lịch sử. Dijkstra giải quyết chọn tuyến theo trọng số; FleetManager gán việc và theo dõi pin/timeout; MutexArbiter điều phối giao lộ; gateway cung cấp giao diện và dữ liệu báo cáo. Các lựa chọn này đáp ứng phạm vi phần mềm của đề tài hai AGV và cho phép mô phỏng, đối soát từng trạng thái. Báo cáo không kết luận hệ thống đã đạt an toàn vận hành hai xe thật; phần đó cần thử nghiệm và tiêu chí nghiệm thu đã chốt.")

    doc.add_page_break()
    doc.add_heading("Tài liệu tham khảo",level=1)
    references=[
        ("P1","docs/00-project-brief.md và docs/requirements/register.md, workspace AGVproject, đối chiếu 07/10/2026."),
        ("P2","docs/19-server-algorithm-flowchart.md, lưu đồ server của workspace, 07/10/2026."),
        ("P3","src/AGVserver.cpp, src/WebCommands.hpp và src/RuntimeStore.hpp, mã Core của workspace, đối chiếu 07/10/2026."),
        ("P4","AGVwebserver/gateway.js, AGVwebserver/db.js và contracts/web-server.md, workspace, đối chiếu 07/10/2026."),
        ("P5","src/FLeetmanager.hpp, src/MapGraph.hpp và docs/05-algorithms.md, workspace, đối chiếu 07/10/2026."),
        ("P6","src/ShiftController.hpp, docs/17-shifts-and-home-charging.md và docs/18-hardening.md, workspace, đối chiếu 07/10/2026."),
        ("P7","src/Mutexarbiter.hpp và docs/07-network.md, workspace, đối chiếu 07/10/2026."),
        ("E1","OASIS Open. MQTT Version 3.1.1, 2014. https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html"),
        ("E2","Fette, I. và Melnikov, A. RFC 6455 The WebSocket Protocol, IETF, 2011. https://datatracker.ietf.org/doc/html/rfc6455"),
        ("E3","Fielding, R. và cộng sự. RFC 9110 HTTP Semantics, IETF, 2022. https://www.rfc-editor.org/rfc/rfc9110.html"),
        ("E4","Dijkstra, E. W. A note on two problems in connexion with graphs. Numerische Mathematik 1, 269–271, 1959. https://doi.org/10.1007/BF01386390"),
        ("E5","SQLite Consortium. Transaction và SQLite Is Transactional. https://www.sqlite.org/lang_transaction.html ; https://www.sqlite.org/transactional.html"),
    ]
    for key, item in references:
        p=doc.add_paragraph()
        p.paragraph_format.left_indent=Cm(0.8)
        p.paragraph_format.first_line_indent=Cm(-0.8)
        p.paragraph_format.space_after=Pt(4)
        p.add_run(f"[{key}] ").bold=True
        p.add_run(item)

    doc.core_properties.title="Thuật toán và kiến trúc server điều phối hai robot AGV trong kho"
    doc.core_properties.subject="Báo cáo giải thích định nghĩa, chức năng, lựa chọn kỹ thuật và sự phù hợp với đề tài AGV"
    doc.save(OUT)
    print(OUT)


if __name__ == "__main__":
    architecture()
    server_loops()
    decision()
    warehouse_map()
    evidence_timeline()
    recovery()
    build_doc()
