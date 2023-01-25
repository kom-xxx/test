#!/usr/bin/env python3

import sys
import cv2

def main():
    i = 0;
#    capture = cv2.VideoCapture("/home/kom/camera_h264.mp4")
    capture = cv2.VideoCapture(0)
    if capture.isOpened() is False:
        raise IOError

    while(capture.isOpened()):
        try:
            ret, frame = capture.read()
            if ret is False:
                break
            print(type(frame), len(frame))
            cv2.imshow('frame',frame)
            '''
            k = cv2.waitKey(100)
            if k == ord('q'):
                break

'''
            i += 1
            if i >= 100:
                break
        except KeyboardInterrupt:
            # 終わるときは CTRL + C を押す
            break

    capture.release()
    cv2.destroyAllWindows()

    return 0

if __name__ == '__main__':
    rc = main()
    sys.exit(rc)
