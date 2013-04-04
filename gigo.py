#!/usr/bin/python -u

import os
import re
import math
import time
import glob
import string
import subprocess

#-------------------------------------------------------------------------------

logging = True

timing  = ''
#timing = ' -t'

#-------------------------------------------------------------------------------

# Return a randomly-named, newly-touched file. We don't use python's hyper-
# paranoid tempfile because the file must outlive various processes, we don't
# want it opened on creation, and we're really not fearful of a race condition.

def mktemp():
    return subprocess.check_output(['mktemp', 'bin.XXXXXX']).rstrip()

# Remove all files that match the temporary file name convention.

def rmtemp():
    for s in glob.glob('bin.??????'):
        os.remove(s)

#-------------------------------------------------------------------------------

# Log a non-empty string to the terminal.

def log(s):
    if logging:
        if len(s) > 0:
            print(s)

# Log the command, execute it, and log its output.

def run(i):
    log(i)
    o = subprocess.check_output(i.split()).rstrip()
    log(o)
    return o

#-------------------------------------------------------------------------------

# Encapsulate image parameters in a tuple.

def imgname(img):
    return img[0]

def imgl(img):
    return img[1]

def imgn(img):
    return img[2]

def imgm(img):
    return img[3]

def imgp(img):
    return img[4]

def imgh(img):
    return 1 << imgn(img)

def imgw(img):
    return 1 << imgm(img)

def imgargs(img):
    return '-l{} -n{} -m{} -p{}'.format(imgl(img), imgn(img), imgm(img), imgp(img))

def imgmake(name, l, n, m, p):
    return (name, l, n, m, p)

def imgtemp(l, n, m, p):
    return (mktemp(), l, n, m, p)

def imgrm(img):
    os.remove(imgname(img))

def imgdupe(img):
    tmp = imgtemp(imgl(img), imgn(img), imgm(img), imgp(img))
    run('cp {} {}'.format(imgname(img), imgname(tmp)))
    return tmp

#-------------------------------------------------------------------------------

# Convert the given tif to an image cache file with a random temporary name.
# Return all image parameters in a tuple.

def toimg(tif):
    tmp = mktemp()
    out = run('convert{} -v  -l5 {} {}'.format(timing, tif, tmp))
    dat = re.search('(\S+) (\d+) (\d+) (\d+) (\d+)', out)
    return imgmake(dat.group(1),
               int(dat.group(2)),
               int(dat.group(3)),
               int(dat.group(4)),
               int(dat.group(5)))

def toimge(tif):
    tmp = mktemp()
    out = run('convert{} -ve -l5 {} {}'.format(timing, tif, tmp))
    dat = re.search('(\S+) (\d+) (\d+) (\d+) (\d+)', out)
    return imgmake(dat.group(1),
               int(dat.group(2)),
               int(dat.group(3)),
               int(dat.group(4)),
               int(dat.group(5)))

# Convert the given image cache to a tif with the given name.

def totif(img, tif):
    run('convert{} {}     {} {}'.format(timing, imgargs(img), imgname(img), tif))

def totifr(img, tif):
    run('convert{} {} -r  {} {}'.format(timing, imgargs(img), imgname(img), tif))

def totifre(img, tif):
    run('convert{} {} -re {} {}'.format(timing, imgargs(img), imgname(img), tif))

#-------------------------------------------------------------------------------

# Reserve space for a destination image.

def reserve(l, n, m, p):
    dst = imgtemp(l, n, m, p)
    run('reserve{} {} -1 {}'.format(timing, imgargs(dst), imgname(dst)))
    return dst

#-------------------------------------------------------------------------------

# Add the source image to the destination.

def add(dst, src):
    run('compute{} {} -A  {} {}'.format(timing, imgargs(dst),
                                                imgname(dst),
                                                imgname(src)))

# Multiply the source image and the destination.

def mul(dst, src):
    run('compute{} {} -M  {} {}'.format(timing, imgargs(dst),
                                                imgname(dst),
                                                imgname(src)))

#-------------------------------------------------------------------------------

# Threshold a range of pixel values.

def select(dst, a, b):
    run('compute{} {} -r{} -R{} {}'.format(timing, imgargs(dst),
                                             a, b, imgname(dst)))

def select_gte(dst, k):
    run('compute{} {} -r{} {}'.format(timing, imgargs(dst),
                                           k, imgname(dst)))

def select_lte(dst, k):
    run('compute{} {} -R{} {}'.format(timing, imgargs(dst),
                                           k, imgname(dst)))

def invert(dst):
    run('compute{} {} -I  {}'.format(timing, imgargs(dst),
                                             imgname(dst)))

#-------------------------------------------------------------------------------

# Filters

def filter_hanning(dst, x, y, r, w):
    run('filter {} {} -x{} -y{} -r{} -w{} -I -H {}'.format(timing, imgargs(dst),
                                                      x, y, r, w, imgname(dst)))

def filter_gaussian(dst, x, y, r):
    run('filter {} {} -x{} -y{} -r{} -G {}'.format(timing, imgargs(dst),
                                                 x, y, r, imgname(dst)))

#-------------------------------------------------------------------------------

# Kernels

def kernel_gaussian(dst, r):
    run('kernel {} {} -r{} -g {}'.format(timing, imgargs(dst), r, imgname(dst)))

def kernel_circle(dst, r):
    run('kernel {} {} -r{} -c {}'.format(timing, imgargs(dst), r, imgname(dst)))

#-------------------------------------------------------------------------------

# Perform a 2D Fourier analysis of the source image.

def fourier2d(dst):
    run('fourier{} {}     {}'.format(timing, imgargs(dst), imgname(dst)))
    run('fourier{} {} -T  {}'.format(timing, imgargs(dst), imgname(dst)))

# Perform a 2D Fourier synthesis of the source image.

def inverse2d(dst):
    run('fourier{} {} -I  {}'.format(timing, imgargs(dst), imgname(dst)))
    run('fourier{} {} -IT {}'.format(timing, imgargs(dst), imgname(dst)))

#-------------------------------------------------------------------------------

# Transfer a centered block of pixels from one image to another.

def transfer(dst, src, W, H):
    x = imgw(dst) / 2 - W / 2
    y = imgh(dst) / 2 - H / 2
    X = imgw(src) / 2 - W / 2
    Y = imgh(src) / 2 - H / 2
    run('transfer{} '.format(timing)
        + '-l{} -n{} -m{} -p{} '.format(imgl(dst), imgn(dst), imgm(dst), imgp(dst))
        + '-L{} -N{} -M{} -P{} '.format(imgl(src), imgn(src), imgm(src), imgp(src))
        + '-x{} -y{} '.format(x, y)
        + '-X{} -Y{} -W{} -H{} '.format(X, Y, W, H)
        + '{} {}'.format(imgname(dst), imgname(src)))

#-------------------------------------------------------------------------------

# Perform frequency-domain morphology using convolution and thresholding.

def dilate(dst, r):
    l   = imgl(dst)
    n   = imgn(dst)
    m   = imgm(dst)
    p   = imgp(dst)
    ker = reserve(l, n, m, p)

    kernel_circle(ker, r)
    fourier2d(ker)
    fourier2d(dst)
    mul(dst, ker)
    inverse2d(dst)
    select_gte(dst, 0.001)

    imgrm(ker)

def erode(dst, r):
    l   = imgl(dst)
    n   = imgn(dst)
    m   = imgm(dst)
    p   = imgp(dst)
    ker = reserve(l, n, m, p)

    kernel_circle(ker, r)
    invert(dst)
    fourier2d(ker)
    fourier2d(dst)
    mul(dst, ker)
    inverse2d(dst)
    invert(dst)
    select_gte(dst, 0.999)

    imgrm(ker)

#-------------------------------------------------------------------------------

# Perform a frequency-domain Gaussian blur using convolution.

def blur(dst, r):
    l   = imgl(dst)
    n   = imgn(dst)
    m   = imgm(dst)
    p   = imgp(dst)
    ker = reserve(l, n, m, p)

    filter_gaussian(ker, (1 << m) / 2,
                         (1 << n) / 2,
                         (1 << n) / math.pi / r)
    fourier2d(dst)
    mul(dst, ker)
    inverse2d(dst)

    imgrm(ker)

#-------------------------------------------------------------------------------
