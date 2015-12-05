#################################################################################################
# This class models a Whistle binary file setup for burner.exe to use.
# Burner spec is here: http://www.freescale.com/files/soft_dev_tools/doc/user_guide/BURNERUG.pdf
# Ernie A (04/26/2013)
#################################################################################################
import io
import os, subprocess
import re

XMAP_IMAGE_START_MATCH_INIT = "  v_addr   p_addr   size     name\r\n  "
XMAP_IMAGE_START_MATCH_TERM = " 00"
XMAP_IMAGE_START_MATCH = XMAP_IMAGE_START_MATCH_INIT + "(.*?)" + XMAP_IMAGE_START_MATCH_TERM

XMAP_IMAGE_LENGTH_MATCH_INIT = "romp\r\n#>"
XMAP_IMAGE_LENGTH_MATCH_TERM = "          __S_romp"
XMAP_IMAGE_LENGTH_MATCH = XMAP_IMAGE_LENGTH_MATCH_INIT + "(.*?)" + XMAP_IMAGE_LENGTH_MATCH_TERM

# NOTE we do not currently use the below methods for determing where the header is.
XMAP_APP_HEADER_START_MATCH_INIT = ".appheader\r\n#>"
XMAP_APP_HEADER_START_MATCH_TERM = "          apphdr"
XMAP_APP_HEADER_START_MATCH = XMAP_APP_HEADER_START_MATCH_INIT + "(.*?)" + XMAP_APP_HEADER_START_MATCH_TERM

XMAP_APP_HEADER_LEN_MATCH_INIT = " "
XMAP_APP_HEADER_LEN_MATCH_TERM = " .appheader"
XMAP_APP_HEADER_LEN_MATCH = XMAP_APP_HEADER_LEN_MATCH_INIT + "(.*?)" + XMAP_APP_HEADER_LEN_MATCH_TERM


SETTING_UNSET = "UNDEFINED"

def wrap_string_param(value_str):
    return '''"''' + value_str + '''"'''

class WhistleBinary():
    'Whistle Binary Model for burner.exe'

    _format = "binary"
    _buswidth = '1'

    # Ernie:When using burner.exe, what is the length of the "range to copy" suppose to be?
    #         It is the length of the .s19 file, or .. ?

    # Jim:  This number minus the orgin (22000) plus  0x20
    #        >00030CE8          __S_romp (linker command file)
    #       I usually just round it up to the end of the enclosing sector.

    # Ernie: What is the downside of making the image length "too large"?

    # Jim:  None, other than taking longer to load. We could just set the sizes to the max available,
    #       5e000 for the app, 10000 for boot2's, 2000 for boot1.

    _len    = SETTING_UNSET # default is 0x33000 hex.
    _origin = SETTING_UNSET # default is for Whistle app (0x22000)
    
    _undefByte = "0xff"  # what the output file will be padded with.
    _sendbyte = '1'
    _binfile =      SETTING_UNSET
    _s19file =      SETTING_UNSET
    _xmapfile =     SETTING_UNSET
    _img_type =     SETTING_UNSET
    _major_ver =    SETTING_UNSET
    _minor_ver =    SETTING_UNSET
    _tag =          SETTING_UNSET

    def setlen(self, thelen):
        "Sets the length of the binary output file"
        self._len = thelen

    def setorigin(self, origin):
        "Sets the origin in memory of where the bin file will be flashed"
        self._origin = origin

    def sets19file(self, s19file):
        "Sets the full pathname of input .s19 file"
        double_quote = '''"'''
        self._s19file = s19file
    def setbinfile(self, binfile):
        "Sets the full pathname of output .bin file"
        self._binfile = binfile

    def setxmap(self, xmap):
        "Sets the full pathname of the input xmap file"
        self._xmapfile = xmap

    def set_image_version(self, major, minor, tag):
        self._major_ver = major
        self._minor_ver = minor
        self._tag = tag

    def parse_xmap_file(self):
        IMG_LEN_ADJUST = 0x100

        fh = io.FileIO(self._xmapfile, "r")
        f_contents = fh.readall()

        image_start_str = re.findall(XMAP_IMAGE_START_MATCH, f_contents)
        if len(image_start_str) != 1:
            print("ERROR: Error parsing image start from XMAP file.")
            print("    f: " + str(self._xmapfile))
            return [None, None]
        image_start = int(image_start_str[0], 16)
        print("Image start location: 0x" + image_start_str[0] + " (" + str(image_start) + ")")

        image_length_str = re.findall(XMAP_IMAGE_LENGTH_MATCH, f_contents)
        image_length = int(image_length_str[0], 16)
        print("Image length\r\n  original from xMAP:\t\t\t0x" + image_length_str[0] + " (" + str(image_length) + ")");
        image_adjusted_length = image_length - image_start
        print("  adjusted for start loc:\t\t" + hex(image_adjusted_length) + " (" + str(image_adjusted_length) + ")")
        image_adjusted_length += 0x100
        print("  adjusted for header padding:\t" + hex(image_adjusted_length) + " (" + str(image_adjusted_length) + ")")

        return [image_start, image_adjusted_length]

    def burn(self):
        "Burns the binary image in accordance with burner.exe batch command reqs"

        if self._xmapfile != SETTING_UNSET:
            [origin, length] = self.parse_xmap_file()
            if origin is None or length is None:
                print ("ERROR: Could not parse xmap file.")
                return -1
        elif self._len != SETTING_UNSET and self._origin != SETTING_UNSET:
            length = self._len
            origin = self._origin
        else:
            print("ERROR: no xmap file provided.\r\n")
            return -1
        
        if self._minor_ver == SETTING_UNSET or self._major_ver == SETTING_UNSET or self._tag == SETTING_UNSET:
            print("ERROR: version information not provided")
            return -1

        temp_bin_file = self._binfile + ".tmp"

        file_out = io.FileIO("err.log", "w")
        cmd = wrap_string_param(os.path.abspath(os.path.dirname(__file__) + "\\" + "burner.exe"))

        cmd += " OPENFILE "  + wrap_string_param(temp_bin_file)
        cmd += " format="    + self._format
        cmd += " busWidth="  + self._buswidth
        cmd += " origin="    + hex(origin)
        cmd += " len="       + hex(length)
        cmd += " undefByte=" + self._undefByte
        cmd += " SENDBYTE "  + self._sendbyte + ' ' + wrap_string_param(self._s19file)

        print '\ncmd\n' + cmd
        proc = subprocess.Popen(cmd, stdout=file_out, stderr=subprocess.PIPE, shell=True)

        # This (not using "shell=True" is supposed the be the proper way to spawn the
        # process however had trouble getting it to work.
        
        # param = [cmd,
        # "OPENFILE", temp_bin_file,
        # "format="+    self._format,
        # "busWidth="+  self._buswidth,
        # "origin="+    hex(origin),
        # "len="+      hex(length),
        # "undefByte="+ self._undefByte,
        # "SENDBYTE", self._sendbyte,
        # self._s19file]
        # print '\nparam\n' + str(param)
        # proc = subprocess.Popen(param, stdout=file_out, stderr=subprocess.PIPE)

        for line in proc.stderr:
            print(line)

        proc.wait()

        print

        print "\nChecking err.log for errors."
        try:
            f = open('err.log', 'r')
            print f.read()
            f.close()
        except:
            print "OK"

        print "Checking EDOUT for details of burn."
        try:
            f = open('EDOUT', 'r')
            contents = f.read()
            f.close()
            if len(contents) == 0:
                print "OK"
            else:
                print contents
        except:
            print "OK"

        HEADER_OFFSET       = 0x200
        HEADER_NAME_OFFSET  = HEADER_OFFSET + 0
        HEADER_VER_OFFEST   = HEADER_OFFSET + 3
        HEADER_CRC_OFFSET   = HEADER_OFFSET + 6
        HEADER_TAG_OFFSET   = HEADER_OFFSET + 16
        HEADER_TAG_MAXLEN   = 16

        bf = io.FileIO(temp_bin_file, "r")
        content_str = bf.readall()
        content_byte = bytearray(content_str)

        bf.close()
        content_byte[HEADER_VER_OFFEST] = 0x00
        content_byte[HEADER_VER_OFFEST + 1] = self._major_ver
        content_byte[HEADER_VER_OFFEST + 2] = self._minor_ver

        tag = ""
        if self._tag != SETTING_UNSET:
            tag = self._tag

        i = 0
        while (i < HEADER_TAG_MAXLEN):
            if i < len(tag):
                content_byte[HEADER_TAG_OFFSET + i] = tag[i]
            else:
                content_byte[HEADER_TAG_OFFSET + i] = 0xFF
            i += 1

        crc = content_byte[HEADER_OFFSET] + \
              content_byte[HEADER_OFFSET + 1] + \
              content_byte[HEADER_OFFSET + 2] + \
              content_byte[HEADER_OFFSET + 3] + \
              content_byte[HEADER_OFFSET + 4] + \
              content_byte[HEADER_OFFSET + 5]

        content_byte[HEADER_CRC_OFFSET] = (-1 * crc) & 0xFF

        bf = io.FileIO(self._binfile, "w")
        bf.write(content_byte)
        bf.close()
