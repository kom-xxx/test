#!/usr/bin/env python3

import sys
import cv2

source = 'file'

def main():
    i = 0;
    if source == 'file':
        capture = cv2.VideoCapture("./camera_h264.mp4")
    else:
        capture = cv2.VideoCapture(0) # specify cam # whwn input is a camera
        ##
        ## set camera parameter using capture.set(...)
        ##

    if capture.isOpened() is False:
        return 1

    while(capture.isOpened()):
        try:
            # capture a frame
            ret, frame = capture.read()  # frame is a ndarray of numpy
            if ret is False:
                break

            ##
            ## Here is the image processing place.
            ## eg. resize, color modify, ... etc.
            ##

            # show a frame
            cv2.imshow('frame',frame)
            if source == 'file':
                # wait for 1/FR when souece is a file
                k = cv2.waitKey(100)  # unit is ms
                if k == ord('q'):
                    break
            i += 1
            if i > 100:
                break

        except KeyboardInterrupt:
            # Ctrl+C pressed
            break

    capture.release()
    cv2.destroyAllWindows()

    return 0

if __name__ == '__main__':
    rc = main()
    sys.exit(rc)
