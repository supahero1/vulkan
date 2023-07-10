import cv2

name = "textures/4x4x3.png"
image = cv2.imread(name, cv2.IMREAD_UNCHANGED)
bgra_image = cv2.cvtColor(image, cv2.COLOR_RGBA2BGRA)
cv2.imwrite(name, bgra_image)
