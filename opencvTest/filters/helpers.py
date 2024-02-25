import cv2 as cv
import numpy as np
from scipy.interpolate import UnivariateSpline


LUTs = {}

def create_LUT_8UC1(x,y):
    spl = UnivariateSpline(x,y)
    return spl(range(256))


def get_lut(percentage):
    if percentage < 0 or percentage > 1.0:
        print("Percentage should be a number between 0 and 1.0, the given number is {}".format(str(percentage)))
        return None
    if percentage not in LUTs.keys():
        LUTs[percentage] = create_LUT_8UC1([0, 4, 8, 16, 32, 64, 128, 156, 196, 228, 256],
        np.clip([0, 4+percentage*4, 8+percentage*8, 16+percentage*16, 32+percentage*32, 64+percentage*64, 128+percentage*128,156+156*percentage, 196+196*percentage, 228+228*percentage, 256], 0, 253))
        
    return LUTs[percentage]
lighten_dark_lut = create_LUT_8UC1([0, 20, 68, 100, 192, 256],
                                  [10, 40, 88, 110,  192, 256])


incr_ch_lut_big = create_LUT_8UC1([0, 64, 128, 192, 256],
                                  [0, 100, 190, 250, 256])

decr_ch_lut_big = create_LUT_8UC1([0, 64, 128, 192, 256],
                                  [0, 5,  64, 130, 162])

incr_ch_lut_mid = create_LUT_8UC1([0, 64, 128, 192, 256],
                              [0, 90, 180, 230, 256])

decr_ch_lut_mid = create_LUT_8UC1([0, 64, 128, 192, 256],
                              [0, 20,  65, 100, 180])

incr_ch_lut_small = create_LUT_8UC1([0, 64, 128, 192, 256],
                              [0, 70, 170, 210, 256])

decr_ch_lut_small = create_LUT_8UC1([0, 64, 128, 192, 256],
                                    [0, 50,  100, 150, 192])


incr_ch_lut_xsmall = create_LUT_8UC1([0, 64, 128, 192, 256],
                                     [0, 68, 140, 200, 256])

decr_ch_lut_xsmall = create_LUT_8UC1([0, 64, 128, 192, 256],
                                    [0, 55,  110, 170, 230])

incr_ch_lut = [incr_ch_lut_big, incr_ch_lut_mid, incr_ch_lut_small,incr_ch_lut_xsmall]
decr_ch_lut = [decr_ch_lut_big, decr_ch_lut_mid, decr_ch_lut_small,decr_ch_lut_xsmall]


def grayfilter(img):
    filt = cv.cvtColor(img, cv.COLOR_RGB2GRAY)
    return filt

def rgbTohsv(img):
    return cv.cvtColor(img, cv.COLOR_RGB2HSV)

def hsvTorgb(hsv_img):
    return cv.cvtColor(hsv_img, cv.COLOR_HSV2RGB)

def rgbTolab(img):
    return cv.cvtColor(img, cv.COLOR_RGB2LAB)

def labTorgb(lab_img):
    return cv.cvtColor(lab_img, cv.COLOR_LAB2RGB)



def RIncrFilter(img, percentage):
    c_r, c_g, c_b = cv.split(img)
    c_r = cv.LUT(c_r, get_lut(percentage)).astype(np.uint8)
    return cv.merge((c_r, c_g, c_b))

def GIncrFilter(img, percentage):
    c_r, c_g, c_b = cv.split(img)
    c_g = cv.LUT(c_g, get_lut(percentage)).astype(np.uint8)
    return cv.merge((c_r, c_g, c_b))

def BIncrFilter(img, percentage):
    c_r, c_g, c_b = cv.split(img)
    c_b = cv.LUT(c_b, get_lut(percentage)).astype(np.uint8)
    return cv.merge((c_r, c_g, c_b))

def RDecrFilter(img, percentage):
    c_r, c_g, c_b = cv.split(img)
    c_r = cv.LUT(c_r, decr_ch_lut_mid).astype(np.uint8)
    return cv.merge((c_r, c_g, c_b))

def GDecrFilter(img, percentage):
    c_r, c_g, c_b = cv.split(img)
    c_g = cv.LUT(c_g, decr_ch_lut_small).astype(np.uint8)
    return cv.merge((c_r, c_g, c_b))

def BDecrFilter(img, percentage):
    c_r, c_g, c_b = cv.split(img)
    c_b = cv.LUT(c_b, decr_ch_lut_small).astype(np.uint8)
    return cv.merge((c_r, c_g, c_b))

def HIncrFilter(hsv_img):
    c_h, c_s, c_v = cv.split(hsv_img)
    c_h = cv.LUT(c_h, incr_ch_lut_small).astype(np.uint8)
    return cv.merge((c_h, c_s, c_v))

def SIncrFilter(hsv_img):
    c_h, c_s, c_v = cv.split(hsv_img)
    c_s = cv.LUT(c_s, incr_ch_lut_big).astype(np.uint8)
    return cv.merge((c_h, c_s, c_v))

def VIncrFilter(hsv_img):
    c_h, c_s, c_v = cv.split(hsv_img)
    c_v = cv.LUT(c_v, incr_ch_lut_small).astype(np.uint8)
    return cv.merge((c_h, c_s, c_v))

def HDecrFilter(hsv_img):
    c_h, c_s, c_v = cv.split(hsv_img)
    c_h = cv.LUT(c_h, decr_ch_lut_small).astype(np.uint8)
    return cv.merge((c_h, c_s, c_v))

def SDecrFilter(hsv_img):
    c_h, c_s, c_v = cv.split(hsv_img)
    c_s = cv.LUT(c_s, decr_ch_lut_small).astype(np.uint8)
    return cv.merge((c_h, c_s, c_v))

def VDecrFilter(hsv_img):
    c_h, c_s, c_v = cv.split(hsv_img)
    c_v = cv.LUT(c_v, decr_ch_lut_small).astype(np.uint8)
    return cv.merge((c_h, c_s, c_v))

def LIncrFilter(lab_img):
    c_l, c_a, c_b = cv.split(lab_img)
    c_l = cv.LUT(c_l, incr_ch_lut_small).astype(np.uint8)
    return cv.merge((c_l, c_a, c_b))

def AIncrFilter(lab_img):
    c_l, c_a, c_b = cv.split(lab_img)
    c_a = cv.LUT(c_a, incr_ch_lut_small).astype(np.uint8)
    return cv.merge((c_l, c_a, c_b))

def BbIncrFilter(lab_img):
    c_l, c_a, c_b = cv.split(lab_img)
    c_b = cv.LUT(c_b, incr_ch_lut_small).astype(np.uint8)
    return cv.merge((c_l, c_a, c_b))

def LDecrFilter(lab_img):
    c_l, c_a, c_b = cv.split(lab_img)
    c_l = cv.LUT(c_l, decr_ch_lut_small).astype(np.uint8)
    return cv.merge((c_l, c_a, c_b))

def ADecrFilter(lab_img):
    c_l, c_a, c_b = cv.split(lab_img)
    c_a = cv.LUT(c_a, decr_ch_lut_small).astype(np.uint8)
    return cv.merge((c_l, c_a, c_b))

def BbDecrFilter(lab_img):
    c_l, c_a, c_b = cv.split(lab_img)
    c_b = cv.LUT(c_b, incr_ch_lut_small).astype(np.uint8)
    return cv.merge((c_l, c_a, c_b))


def GrayIncrFilter(gray_img, percentage):
    return cv.LUT(gray_img, get_lut(percentage)).astype(np.uint8)

def GrayDecrFilter(gray_img):
    return cv.LUT(gray_img, decr_ch_lut_big).astype(np.uint8)

def Lighten(gray_img):
    return cv.LUT(gray_img, lighten_dark_lut).astype(np.uint8)


def contrastIncr(img, clipLimit, tilegridx, tilegridy):
    lab= cv.cvtColor(img, cv.COLOR_BGR2LAB)
    l_channel, a, b = cv.split(lab)

    # Applying CLAHE to L-channel
    # feel free to try different values for the limit and grid size:
    clahe = cv.createCLAHE(clipLimit=clipLimit, tileGridSize=(tilegridx,tilegridy))
    cl = clahe.apply(l_channel)

    # merge the CLAHE enhanced L-channel with the a and b channel
    limg = cv.merge((cl,a,b))

    # Converting image from LAB Color model to BGR color spcae
    return cv.cvtColor(limg, cv.COLOR_LAB2BGR)
    
    # Stacking the original image with the enhanced image
    # np.hstack((img, enhanced_img))