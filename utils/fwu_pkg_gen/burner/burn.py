#############################################################
# Use burner.exe to convert an .s19 file to a .bin file.
# Ernie A. 4/28/2013
#############################################################

import sys, os
from optparse import OptionParser
from whistle_binary import WhistleBinary

def parse_hex_dec_option(value_str):
    if value_str.find("0x") < 0:
        length = int(value_str)
    elif value_str.find("0x") == 0:
        length = int(value_str, 16)
    else:
        return None


def burn(opt):
    if opt.s19 is None:
        print ("ERROR: no S19 file provided.")
        return -1
    if not ((opt.xmap is not None) or (opt.len is not None and opt.origin is not None)):
        print ("ERROR: must provide either xmap file or length + origin.")
        return -1

    "Burn the image using options"
    wbin = WhistleBinary()
    if opt.xmap is not None:
        xmap_filepath = os.path.abspath(opt.xmap)
        if xmap_filepath is None:
            print("ERROR: trouble finding xmap file name (1).")
            return -1
        if not os.path.exists(xmap_filepath):
            print("ERROR: trouble finding xmap file (2).")
            return -1
        wbin.setxmap(xmap_filepath)
    else:
        length = parse_hex_dec_option(opt.len)
        if length is None:
            print("ERROR parsing length.")
            return -1
        wbin.setlen(length)

        origin = parse_hex_dec_option(opt.origin)
        if origin is None:
            print("ERROR parsing length.")
            return -1
        wbin.setorigin(origin)

    s19_filepath = os.path.abspath(opt.s19)
    if s19_filepath is None:
        print("ERROR: trouble finding s19 file name (1).")
        return -1
    if not os.path.exists(s19_filepath):
        print("ERROR: trouble finding s19 file (2).")
        return -1
    wbin.sets19file(s19_filepath)

    bfile_path = os.path.abspath(opt.bfile)
    if bfile_path is None:
        print("ERROR: trouble setting binary output file path (1).")
        return -1
    wbin.setbinfile(bfile_path)

    try:
        major = opt.version.split('.')[0]
        minor = opt.version.split('.')[1].split('-')[0]
        tag = opt.version.split('.')[1].split('-')[1]
        print major, minor, tag
    except:
        print "ERROR parsing version.  Use format X.Y-TAG"
        sys.exit()
    
    wbin.set_image_version(major, minor, tag)

    res = wbin.burn()

    # Check for presence of output file to confirm success.
    if not os.path.exists(opt.bfile):
        print "** Error: Output File:", opt.bfile, "could not be found!"
    
    del(wbin)

def options(argv):
    "Parse options"

    parser = OptionParser()
    parser.add_option("-s", "--s19", dest="s19", metavar="FILE.s19", help="path to s19 in file")
    parser.add_option("-x", "--xmap", dest="xmap", metavar="FILE.xMAP", help="path to xMAP in file")
    parser.add_option("-b", "--bin", dest="bfile", metavar="FILE.bin", help="path to bin out file")
    parser.add_option("-l", "--length", dest="len", metavar="LENGTH", help="length of output")
    parser.add_option("-o", "--origin", dest="origin", metavar="ORIGIN", help="origin of output")
    parser.add_option("-v", "--version", dest="version", metavar="MAJOR.MINOR-TAG", help="x.y-tag version for bin")
    opt, args = parser.parse_args(argv)

    if opt.s19 is None:
        sys.exit("s19 input file is required")
    if opt.bfile is None:
        sys.exit("bin output file is required")
    if opt.version is None:
        sys.exit("version is necessary")

    if not os.path.exists(opt.s19):
        print "Error: ", opt.s19, "is not a valid path!"
        sys.exit(0)

    return opt

if __name__ == "__main__":
    "entry point"
    opt = options(sys.argv)
    burn(opt)
