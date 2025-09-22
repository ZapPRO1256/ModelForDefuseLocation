from PIL import Image
import base64

# Вхідний файл
input_file = "m.png"
# Вихідний хедер
output_file = "photo.h"
# Новий розмір фото
resize_width = 320
resize_height = 240

# 1. Відкриваємо і ресайзимо
img = Image.open(input_file)
img = img.resize((resize_width, resize_height), Image.LANCZOS)

# 2. Зберігаємо в буфер як JPEG (можна PNG, але JPEG легший)
from io import BytesIO
buffer = BytesIO()
img.save(buffer, format="PNG")
byte_data = buffer.getvalue()

# 3. Конвертуємо у base64
b64_str = base64.b64encode(byte_data).decode("utf-8")

# 4. Формуємо photo.h
with open(output_file, "w") as f:
    f.write("// Auto-generated photo.h\n")
    f.write("#pragma once\n\n")
    f.write("const char* photo_base64 = \n")
    # Робимо розбиття рядка, щоб не було надто довгих ліній у .h файлі
    for i in range(0, len(b64_str), 80):
        chunk = b64_str[i:i+80]
        f.write(f"\"{chunk}\"\n")
    f.write(";\n")

print(f"[OK] Generated {output_file} with resized photo {resize_width}x{resize_height}")
