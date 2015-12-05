#############################################################
# gluer: glues boot1, boot2a, boot2b, and an app together.
# Ernie A. 11/04/2013
#############################################################

import sys, os
from optparse import OptionParser

# Where the images live in K60 Flash
B1_ADDR  = int(0x0)
B2A_ADDR = int(0x00002000)
B2B_ADDR = int(0x00012000)
APP_ADDR = int(0x00022000)

# 512KB
K60_FLASH_LEN = 0x00080000  

# Lengths used to determine how much padding we need.
B1_LEN   = int(B2A_ADDR) - int(B1_ADDR)
B2A_LEN  = int(B2B_ADDR) - int(B2A_ADDR)
B2B_LEN  = int(APP_ADDR) - int(B2B_ADDR)
APP_LEN  = int(K60_FLASH_LEN) - int(APP_ADDR)

def chk_sz(the_path, max_sz):
    "Check size of the given file to make sure its OK"

    the_sz = os.path.getsize(the_path)
    print the_path, "\tLEN: ", hex(the_sz), "\tMAX: ", hex(max_sz)
    
    if the_sz > max_sz:
        sys.exit("ERR: LAST BIN TOO BIG")
    return the_sz

def write_bin(the_path, out_file, padding):
    "Writes to the output bin file"

    the_file = open(the_path, 'rb')
    contents = the_file.read()
    the_file.close()

    if not padding is None:
        contents = contents = contents.ljust(padding, '\xFF')

    out_file.write(contents)

def glue(opt):
    "Glue the image using options"

    # Get sizes for files, and make sure within range.
    b1_sz  = chk_sz(opt.boot1,  B1_LEN)
    b2a_sz = chk_sz(opt.boot2a, B2A_LEN)
    b2b_sz = chk_sz(opt.boot2b, B2B_LEN)
    app_sz = chk_sz(opt.app,    APP_LEN)

    # Open up the bin file for writing.
    binfile = open(opt.output, 'wb')

    # Write all the individual bins to this file, with padding (except app).
    write_bin(opt.boot1,  binfile, B1_LEN)
    write_bin(opt.boot2a, binfile, B2A_LEN)
    write_bin(opt.boot2b, binfile, B2B_LEN)
    write_bin(opt.app,    binfile, None)

    print "Output:\t", opt.output
    binfile.close()
    
def options(argv):
    "Parse options"

    parser = OptionParser()
    parser.add_option("-b", "--boot1",  dest="boot1",  metavar="0.1-BOOT1.bin",  help="boot1 bin file input")
    parser.add_option("-1", "--boot2a", dest="boot2a", metavar="0.1-BOOT2A.bin", help="boot2a bin file input")
    parser.add_option("-2", "--boot2b", dest="boot2b", metavar="0.1-BOOT2B.bin", help="boot2b bin file input")
    parser.add_option("-a", "--app",    dest="app",    metavar="0.2-APP.bin",    help="application bin file input")
    parser.add_option("-o", "--output", dest="output", metavar="B2P.BIN",        help="glued together output bin")
    opt, args = parser.parse_args(argv)

    if opt.boot1 is None or not os.path.exists(opt.boot1):
        sys.exit("boot1 input file is required")
    if opt.boot2a is None or not os.path.exists(opt.boot2a):
        sys.exit("boot2a input file is required")
    if opt.boot2b is None or not os.path.exists(opt.boot2b):
        sys.exit("boot2b input file is required")
    if opt.app is None or not os.path.exists(opt.app):
        sys.exit("app input file is required")
    if opt.output is None:
        sys.exit("output path is required")
    return opt

if __name__ == "__main__":
    "entry point"
    print "\n",
    opt = options(sys.argv)
    glue(opt)
