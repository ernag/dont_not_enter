import os, sys
from optparse import OptionParser
import shlex
import shutil
import subprocess
import re
import base64
import binascii
import struct
import io

sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))
from fwu_pkg_gen.burner.whistle_binary import WhistleBinary
from fwu_pkg_gen.fwu_pkg_gen import FwuPackageGenerator
from fwu_pkg_gen.tlv import BinaryImageTLV

CODEWARRIOR_PATH_DEFAULT = "C:\\Freescale\\CW MCU v10.3"
_ECD_REL_PATH = "\\eclipse\\ecd.exe"
ECLIPSE_LOCAL_METADATA = ".metadata"

CONFIG = "config"
CONFIG_D_USB = "d_usb"
CONFIG_D_UART = "d_uart"
REL_PATH = "rel_path"
DEPENDENCY = "dependency"
STATUS = "status"
STATUS_NONE = 0
STATUS_CLEANING = 1
STATUS_CLEAN = 2
STATUS_BUILDING = 3
STATUS_BUILT = 4
STATUS_ERROR = 5
TITLE = "title"
OUT_BASE = "out_base"
GEN_S19_FILE_LOC = "gen_s19_loc"
GEN_XMAP_FILE_LOC = "gen_xmap_loc"
GEN_BIN_FILE_LOC = "gen_bin_loc"
VERSION = "ver"

CORONA_FW_LOC = ""

BT_PATCH_DEF_LOCATION = CORONA_FW_LOC + "\\MQX 3.8\\whistle\\corona\\drivers\\bt\\Bluetopia\\btpsvend"
BT_BASE_PATCH_FILE = "BasePatch.h"
BT_LOW_ENERGY_PATCH_FILE = "LowEnergyPatch.h"


BSP_BUILD_ITEM = {TITLE: "BSP",
                  CONFIG:       {CONFIG_D_UART:"Debug UART", CONFIG_D_USB : "Debug USB"},
                  REL_PATH:     CORONA_FW_LOC + "\\MQX 3.8\\mqx\\build\\cw10\\bsp_coronak60n512",
                  OUT_BASE :    None,
                  DEPENDENCY:   [],
                  STATUS :      STATUS_NONE}

PSP_BUILD_ITEM = {TITLE: "PSP",
                  CONFIG :      {CONFIG_D_UART:"Debug UART", CONFIG_D_USB : "Debug USB"},
                  REL_PATH :    CORONA_FW_LOC + "\\MQX 3.8\\mqx\\build\\cw10\\psp_coronak60n512",
                  OUT_BASE :    None,
                  DEPENDENCY :  [BSP_BUILD_ITEM],
                  STATUS :      STATUS_NONE}

CFGFAC_BUILD_ITEM = {TITLE: "CFGFAC",
                  CONFIG :      {CONFIG_D_UART:"Debug", CONFIG_D_USB : "Debug"},
                  REL_PATH :    CORONA_FW_LOC + "\\MQX 3.8\\projects\\cfg_factory\\cfg_factory",
                  OUT_BASE :    None,
                  DEPENDENCY :  [],
                  STATUS :      STATUS_NONE}

SHERLOCK_BUILD_ITEM = {TITLE: "SHERLOCK",
                  CONFIG :      {CONFIG_D_UART:"Debug", CONFIG_D_USB : "Debug"},
                  REL_PATH :    CORONA_FW_LOC + "\\MQX 3.8\\projects\\sherlock",
                  OUT_BASE :    None,
                  DEPENDENCY :  [],
                  STATUS :      STATUS_NONE}

PMEM_BUILD_ITEM = {TITLE: "PMEM",
                  CONFIG :      {CONFIG_D_UART:"Debug", CONFIG_D_USB : "Debug"},
                  REL_PATH :    CORONA_FW_LOC + "\\MQX 3.8\\projects\\pmem",
                  OUT_BASE :    None,
                  DEPENDENCY :  [],
                  STATUS :      STATUS_NONE}

RTCS_BUILD_ITEM = {TITLE: "RTCS",
                  CONFIG:       {CONFIG_D_UART:"Debug", CONFIG_D_USB : "Debug"},
                  REL_PATH:     CORONA_FW_LOC + "\\MQX 3.8\\rtcs\\build\\cw10\\rtcs_coronak60n512",
                  OUT_BASE :    None,
                  DEPENDENCY:   [BSP_BUILD_ITEM, PSP_BUILD_ITEM],
                  STATUS :      STATUS_NONE}

USBDDK_BUILD_ITEM = {TITLE: "USBDDK",
                  CONFIG:       {CONFIG_D_UART:"Debug", CONFIG_D_USB : "Debug"},
                  REL_PATH:     CORONA_FW_LOC + "\\MQX 3.8\\usb\\device\\build\\cw10\\usb_ddk_coronak60n512",
                  OUT_BASE :    None,
                  DEPENDENCY:   [BSP_BUILD_ITEM, PSP_BUILD_ITEM],
                  STATUS :      STATUS_NONE}

UTF_BUILD_ITEM = {TITLE: "UTF",
                  CONFIG:       {CONFIG_D_UART:"Debug", CONFIG_D_USB : "Debug"},
                  REL_PATH:     CORONA_FW_LOC + "\\MQX 3.8\\usb\\device\\build\\cw10\\usb_ddk_coronak60n512",
                  OUT_BASE :    {CONFIG_D_UART: CORONA_FW_LOC + "\\MQX 3.8\\projects\\atheros_utf_agent_corona\\utf_agent\\cw10\\utf_agent_coronak60n512\\Int Flash Debug\\intflash_d.afx",
                                 CONFIG_D_USB: CORONA_FW_LOC + "\\MQX 3.8\\projects\\atheros_utf_agent_corona\\utf_agent\\cw10\\utf_agent_coronak60n512\\Int Flash Debug\\intflash_d.afx"},
                  DEPENDENCY:   [BSP_BUILD_ITEM, PSP_BUILD_ITEM, RTCS_BUILD_ITEM, USBDDK_BUILD_ITEM],
                  STATUS :      STATUS_NONE}

APP_BUILD_ITEM = {TITLE: "APP",
                  CONFIG:       {CONFIG_D_UART:"Debug UART", CONFIG_D_USB : "Debug USB"},
                  REL_PATH:     CORONA_FW_LOC + "\\MQX 3.8\\whistle\\corona\\cw10\\corona_coronak60n512",
                  OUT_BASE :    {CONFIG_D_UART :     CORONA_FW_LOC + "\\MQX 3.8\\whistle\\corona\\cw10\\corona_coronak60n512\\Int Flash Debug\\intflash_d_uart.afx",
                                 CONFIG_D_USB :   CORONA_FW_LOC + "\\MQX 3.8\\whistle\\corona\\cw10\\corona_coronak60n512\\Flash USB\\intflash_d_usb.afx" },
                  DEPENDENCY:   [BSP_BUILD_ITEM, PSP_BUILD_ITEM, RTCS_BUILD_ITEM, USBDDK_BUILD_ITEM, CFGFAC_BUILD_ITEM, SHERLOCK_BUILD_ITEM, PMEM_BUILD_ITEM],
                  STATUS :      STATUS_NONE}

BOOT1_BUILD_ITEM = {TITLE: "BOOT1",
                  CONFIG:       {CONFIG_D_UART:"FLASH", CONFIG_D_USB : "FLASH"},
                  REL_PATH:     CORONA_FW_LOC + "\\MQX 3.8\\projects\\boot1",
                  OUT_BASE :    {CONFIG_D_UART :     CORONA_FW_LOC + "\\MQX 3.8\\projects\\boot1\\FLASH\\boot1.afx",
                                 CONFIG_D_USB :   CORONA_FW_LOC + "\\MQX 3.8\\projects\\boot1\\FLASH\\boot1.afx" },
                  DEPENDENCY:   [],
                  STATUS :      STATUS_NONE}

BOOT2A_BUILD_ITEM = {TITLE: "BOOT2A",
                  CONFIG:       {CONFIG_D_UART:"FLASH-A", CONFIG_D_USB : "FLASH-A"},
                  REL_PATH:     CORONA_FW_LOC + "\\MQX 3.8\\projects\\boot2",
                  OUT_BASE :    {CONFIG_D_UART :     CORONA_FW_LOC + "\\MQX 3.8\\projects\\boot2\\FLASH\\boot2A.afx",
                                 CONFIG_D_USB :   CORONA_FW_LOC + "\\MQX 3.8\\projects\\boot2\\FLASH\\boot2A.afx" },
                  DEPENDENCY:   [CFGFAC_BUILD_ITEM, SHERLOCK_BUILD_ITEM, PMEM_BUILD_ITEM],
                  STATUS :      STATUS_NONE}

BOOT2B_BUILD_ITEM = {TITLE: "BOOT2B",
                  CONFIG:       {CONFIG_D_UART:"FLASH-B", CONFIG_D_USB : "FLASH-B"},
                  REL_PATH:     CORONA_FW_LOC + "\\MQX 3.8\\projects\\boot2",
                  OUT_BASE :    {CONFIG_D_UART :     CORONA_FW_LOC + "\\MQX 3.8\\projects\\boot2\\FLASH-B\\boot2B.afx",
                                 CONFIG_D_USB :   CORONA_FW_LOC + "\\MQX 3.8\\projects\\boot2\\FLASH-B\\boot2B.afx" },
                  DEPENDENCY:   [CFGFAC_BUILD_ITEM, SHERLOCK_BUILD_ITEM, PMEM_BUILD_ITEM],
                  STATUS :      STATUS_NONE}

BOOT2_BUILD_ITEM = {TITLE: "BOOT2",
                  CONFIG:       None,
                  REL_PATH:     None,
                  OUT_BASE :    None,
                  DEPENDENCY:   [BOOT2A_BUILD_ITEM, BOOT2B_BUILD_ITEM],
                  STATUS :      STATUS_NONE}

FNULL = open(os.devnull, 'w')

def main():
    builder = Builder()
    return builder.run(sys.argv)

class Builder(object):
    _process_dependencies = True
    _ecd_path = None
    _corona_base_path = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
    print "Using corona_fw path: " + _corona_base_path
    _codewarrior_path = os.getenv("WHISTLE_CODEWARRIOR", CODEWARRIOR_PATH_DEFAULT)
    print "Using CodeWarrior path: " + _codewarrior_path
    _build_output = {}
    res = 0

    def run(self, argv):
        parser = OptionParser()
        self.setupOptions(parser)
        (opt, args) = parser.parse_args(argv)

        if os.path.exists(ECLIPSE_LOCAL_METADATA):
            # Freescale mentioned this is necessary to avoid random JAVA errors.
            print "Cleaning CW env file: ", ECLIPSE_LOCAL_METADATA
            shutil.rmtree(ECLIPSE_LOCAL_METADATA)

        self._ecd_path = self._codewarrior_path + _ECD_REL_PATH

        if self.validate_args(opt) == -1:
            print ("Builder: RUN ERROR: failed to validate arguements.\r\n")
            return -1

        if opt.nodep is not None:
            self._process_dependencies = not opt.nodep

        if opt.d_uart:
            config = CONFIG_D_UART
        elif opt.d_usb:
            config = CONFIG_D_USB
        else:
            config = None

# Build projects
        if opt.app:
            if self.build(APP_BUILD_ITEM, config, opt.app, skipbuild=opt.skipbuild) != 0:
                print("\r\nBUILDER: ERROR out because of app build issue.\r\n")
                return -1

        if opt.boot1:
            if self.build(BOOT1_BUILD_ITEM, config, opt.boot1, skipbuild=opt.skipbuild) != 0:
                print("\r\nBUILDER: ERROR out because of boot1 build issue.\r\n")
                return -1

        if opt.boot2:
            if self.build(BOOT2_BUILD_ITEM, config, opt.boot2, skipbuild=opt.skipbuild) != 0:
                print("\r\nBUILDER: ERROR out because of boot2 build issue.\r\n")
                return -1

# Parse C-array and create binary file
        if opt.basepatch:
            in_patch_c = self._corona_base_path + BT_PATCH_DEF_LOCATION + "\\" + BT_BASE_PATCH_FILE
            out_patch_bin = self._out_dir + "\\" + os.path.basename(in_patch_c).rstrip('.c') + "-" + opt.basepatch + ".bin"
            bt_patch_out_file = self.patch_binary_parser(in_patch_c, out_patch_bin)
            print("Parsing BASEPATCH patch C-array and creating bin file.")
            print("        BASEPATCH: " + bt_patch_out_file)
        
        if opt.blepatch:
            in_patch_c = self._corona_base_path + BT_PATCH_DEF_LOCATION + "\\" + BT_LOW_ENERGY_PATCH_FILE
            out_patch_bin = self._out_dir + "\\" + os.path.basename(in_patch_c).rstrip('.c') + "-" + opt.blepatch + ".bin"
            ble_patch_out_file = self.patch_binary_parser(in_patch_c, out_patch_bin)
            print("Parsing LOWENERGY patch C-array and creating bin file.")
            print("        LOWENERGY: " + ble_patch_out_file)

# Convert S19 build outputs to BIN files
        if opt.genpkg or opt.genbin:
            for pkg in self._build_output:
                wbin = WhistleBinary()
                bin_file_path = self._out_dir + "\\" + pkg + "_" + self._build_output[pkg][VERSION] + "_" + config + ".bin"
                wbin.setxmap(self._build_output[pkg][GEN_XMAP_FILE_LOC])
                wbin.sets19file(self._build_output[pkg][GEN_S19_FILE_LOC])
                wbin.setbinfile(bin_file_path)
                [major, minor, tag] = BinaryImageTLV.parse_version_string(self._build_output[pkg][VERSION])
                wbin.set_image_version(major, minor, tag)
                res = wbin.burn()

                # Check for presence of output file to confirm success.
                if not os.path.exists(bin_file_path):
                    print "** Error: Output File: '", bin_file_path, "' could not be found!"
                    return -1

                self._build_output[pkg].update({GEN_BIN_FILE_LOC : bin_file_path})

        if opt.genpkg:
            fwu_pkg_gen_args = ["fwu_pkg_gen.py"]
            pkg_file_path = ""
            if opt.title:
                fwu_pkg_gen_args += ["-t", opt.pkg_title]
                pkg_file_path += opt.title + "_"
            if opt.boot1:
                fwu_pkg_gen_args += ["--boot1-img",
                                     self._build_output[BOOT1_BUILD_ITEM[TITLE]][GEN_BIN_FILE_LOC]]
                fwu_pkg_gen_args += ["--boot1-ver", opt.boot1]
                pkg_file_path += "BOOT1_" + opt.boot1 + "_"
            if opt.boot2:
                fwu_pkg_gen_args += ["--boot2a-img",
                                     self._build_output[BOOT2A_BUILD_ITEM[TITLE]][GEN_BIN_FILE_LOC]]
                fwu_pkg_gen_args += ["--boot2a-ver", opt.boot2]
                fwu_pkg_gen_args += ["--boot2b-img",
                                     self._build_output[BOOT2B_BUILD_ITEM[TITLE]][GEN_BIN_FILE_LOC]]
                fwu_pkg_gen_args += ["--boot2b-ver", opt.boot2]
                pkg_file_path += "BOOT2_" + opt.boot2 + "_"
            if opt.app:
                fwu_pkg_gen_args += ["--app-img",
                                     self._build_output[APP_BUILD_ITEM[TITLE]][GEN_BIN_FILE_LOC]]
                fwu_pkg_gen_args += ["--app-ver", opt.app]
                pkg_file_path += "APP_" + opt.app + "_"
            if opt.pad:
                fwu_pkg_gen_args += ["--pad"]
                pkg_file_path += "PADDED_"
            if opt.basepatch:
                patch_bin_file = bt_patch_out_file
                fwu_pkg_gen_args += ["--basepatch-bin", patch_bin_file]
                pkg_file_path += "BASEPATCH_" + opt.basepatch + "_"
            if opt.blepatch:
                patch_bin_file = ble_patch_out_file
                fwu_pkg_gen_args += ["--blepatch-bin", patch_bin_file]
                pkg_file_path += "BLEPATCH_" + opt.blepatch + "_"

            if pkg_file_path[len(pkg_file_path)-1] == "_":
                pkg_file_path = pkg_file_path[0:len(pkg_file_path)-1]

            pkg_file_path = self._out_dir + "\\" + pkg_file_path + ("" if config is None else ("_" + config)) + ".pkg"
            fwu_pkg_gen_args += ["-o", pkg_file_path]

            print ("FWU PKG GEN ARGS:")
            print (fwu_pkg_gen_args)
            pkgGen = FwuPackageGenerator()
            res = pkgGen.run(fwu_pkg_gen_args)

        print ("\r\n\r\nCOMPLETE!\r\n")
        return res

    def validate_args(self, opt):
        res = 0

        ver_validator = re.compile('[\d]+\.[\d]+\-[\w]+')

        if not os.path.exists(self._codewarrior_path):
            print ("ERROR: could not find code warrior path. This is controlled by environmental\r\n"
                   " variable WHISTLE_CODEWARRIOR. To set this up follow these instructions: \r\n"
                   "  1. Go to Start Menu -> Right Click on My Computer -> Click Properties\r\n"
                   "  2. Click 'Advanced system settings' on the left\r\n"
                   "  3. Click 'Environment Variables' button\r\n"
                   "  4. Click 'New' button\r\n"
                   "  5. Enter variable name of 'WHISTLE_CODEWARRIOR' and variable value\r\n"
                   "     of your path for code warrior\r\n")
            print ("  supplied path: " + self._ecd_path)
            res = -1

        if not os.path.exists(self._ecd_path):
            print ("ERROR: could not find ECD.exe")
            print ("  supplied path: " + self._ecd_path)
            res = -1

        if opt.odir is None:
            print ("ERROR: must supply output directory (--odir).")
            res = -1

        out_dir = os.path.abspath(opt.odir)
        if out_dir is None or not os.path.exists(out_dir):
            print ("ERROR: problem with accessing output directory.")
            print (" dir: " + opt.odir)
            res = -1
        self._out_dir = out_dir

        if opt.boot1 or opt.boot2 or opt.app:
            if opt.d_usb and opt.d_uart:
                print("ERROR: cannot specify both debug UART and debug USB options, choose one.")
                res = -1

            if not opt.d_usb and not opt.d_uart:
                print("ERROR: must specify either --d_usb or --d_uart.")
                res = -1

        if opt.app:
            ver_match = ver_validator.match(opt.app)
            if ver_match is None or ver_match.group() is None:
                print("ERROR: must specify app version when building App in format of <major ver>.<minor ver>-tag "
                      "(#.#-string)")
                res = -1

        if opt.app:
            try:
                x = BinaryImageTLV.parse_version_string(opt.app)
            except:
                print("ERROR: app version does not meet input patter (x.y-tag)")
                res = -1

        if opt.boot1:
            ver_match = ver_validator.match(opt.boot1)
            if ver_match.group() is None:
                print("ERROR: must specify app version when building Boot1 in format of <major ver>.<minor ver>-tag "
                      "(#.#-string)")
                res = -1

        if opt.boot1:
            try:
                x = BinaryImageTLV.parse_version_string(opt.boot1)
            except:
                print("ERROR: boot1 version does not meet input patter (x.y-tag)")
                res = -1

        if opt.boot2:
            ver_match = ver_validator.match(opt.boot2)
            if ver_match.group() is None:
                print("ERROR: must specify app version when building Boot2 in format of <major ver>.<minor ver>-tag "
                      "(#.#-string)")
                res = -1

        if opt.boot2:
            try:
                x = BinaryImageTLV.parse_version_string(opt.boot2)
            except:
                print("ERROR: boot2 version does not meet input patter (x.y-tag)")
                res = -1

        return res

    def setupOptions(self, parser):
        parser.add_option("--app", dest="app",
                          default=None,
                          help="Build the application image with specified version")
        parser.add_option("--boot1", dest="boot1",
                          default=None,
                          help="Build the boot1 image with specified version")
        parser.add_option("--boot2", dest="boot2",
                          default=None,
                          help="Build the boot2 images with specified version")
        parser.add_option("--basepatch", dest="basepatch",
                          default=None,
                          help="Path to the basepatch C-array file")
        parser.add_option("--blepatch", dest="blepatch",
                          default=None,
                          help="Path to the blepatch C-array file")
        parser.add_option("--title",
                          default=None,
                          help="package title")
        parser.add_option("--nodep", dest="nodep",
                          default=False,
                          help="ignore dependencies, build only the image specified",
                          action="store_true")
        parser.add_option("--d_uart", dest="d_uart",
                          default=False,
                          help="Build debug UART configuration",
                          action="store_true")
        parser.add_option("--d_usb", dest="d_usb",
                          default=False,
                          help="Build debug USB configuration",
                          action="store_true")
        parser.add_option("--odir", dest="odir",
                          default=None,
                          help="directory where output files are placed")
        parser.add_option("--genpkg", dest="genpkg",
                          default=False,
                          help="whether to build pkg file (includes building binaries)",
                          action="store_true")
        parser.add_option("--genbin", dest="genbin",
                          default=False,
                          help="whether to build bin files (but not pkg)",
                          action="store_true")
        parser.add_option("--skipbuild", dest="skipbuild",
                          default=False,
                          help="whether to build bin files (but not pkg)",
                          action="store_true")
        parser.add_option("--pad", dest="pad",
                          default=False,
                          help="appends 1500 bytes to the end of the pkg. Needed for devices that have Atheros bug "
                               "that looses the payload of the final TCP packet if it is a FIN",
                          action="store_true")


    def build(self, build_item, config_type, version, skipbuild=False):
        res = 0

        print (" ")
        print ("Starting build process for " + build_item[TITLE])

        # Run through dependencies first
        if self._process_dependencies:
            for dep in build_item[DEPENDENCY]:
                if dep[STATUS] != STATUS_BUILT:
                    print("Processing dependency...")
                    res = self.build(dep, config_type, version, skipbuild)
                    if res != 0:
                        print ("ERROR: dependency build failed")
                        return -1
                else:
                    print("Dependency " + dep[TITLE] + " already built, skipping :)")
        else:
            print("Skipping dependencies")

        if build_item[REL_PATH] is None:
            print ("BUILD COMPLETE")
            return 0
        proj_path = self._corona_base_path + build_item[REL_PATH]

        if not os.path.exists(proj_path):
            print ("ERROR: could not find project folder")
            print ("  supplied path: " + proj_path)
            return -1

        cmd = wrap_string_param(self._ecd_path)
        cmd += " -build -project " + wrap_string_param(proj_path)
        cmd += " -config " + wrap_string_param(build_item[CONFIG][config_type])
        cmd += " -cleanBuild"

        args = shlex.split(cmd)

        if not skipbuild:
            print "Kicking off build for " + build_item[TITLE]
            print cmd
            proc = subprocess.Popen(args, stdout=FNULL)
            build_item[STATUS] = STATUS_BUILDING
            proc.wait()

            if proc.returncode == 0:
                print "Build completed for " + build_item[TITLE] + ". Return code: " + str(proc.returncode)
                build_item[STATUS] = STATUS_BUILT
            else:
                print "Build failed for " + build_item[TITLE] + ". Return code: " + str(proc.returncode)
                build_item[STATUS] = STATUS_ERROR
                return proc.returncode
        else:
            print "Skipping build for " + build_item[TITLE]
            build_item[STATUS] = STATUS_BUILT

        if build_item[OUT_BASE] is not None:
            src_s19_file = os.path.abspath(self._corona_base_path + build_item[OUT_BASE][config_type] + ".S19")
            dest_s19_file = self._out_dir + "\\" + build_item[TITLE] + "_"  + version + "_" + config_type + ".S19"
            shutil.copy(src_s19_file, dest_s19_file)

            src_xmap_file = os.path.abspath(self._corona_base_path + build_item[OUT_BASE][config_type] + ".xMAP")
            dest_xmap_file = self._out_dir + "\\" + build_item[TITLE] + "_"  + version + "_" + config_type + ".xMAP"
            shutil.copy(src_xmap_file, dest_xmap_file)
            self._build_output.update({build_item[TITLE] : {
                                       GEN_S19_FILE_LOC: dest_s19_file,
                                       GEN_XMAP_FILE_LOC: dest_xmap_file,
                                       VERSION: version}})
        return 0

    def patch_binary_parser(self, input_c_file, output_bin_file):
        f = io.open(input_c_file, "r")

        f_read = ""
        lines = f.readlines()
        for line in lines:
            if "//" in line:
                f_read += line[:line.index("//")]
            else:
                f_read += line
        
        count = 0
        hexChars = f_read.split();
        hexCharsList = []
        for hexChar in hexChars:
            hexCharsList += hexChar.split(',')
        outputList = []
        for i in hexCharsList:
            if ("" != i) and i.startswith("0x"):
                outputList.append(i.upper())

        data = ""
        for i in range(len(outputList)):
            try:
                data += base64.b16decode(outputList[i][2:])
            except TypeError:
                print "ERROR: C-array parsing error. Non-hex digit encountered \"" + outputList[i] + "\""

        crc = binascii.crc32(data) & 0xFFFFFFFF
        crc = self.reverseEndianness(crc, 1)
        patchLen = self.reverseEndianness(len(outputList), 0)
        magicNum = self.reverseEndianness(int(0xDEADBEEF), 1)
        
        dataHdr = base64.b16decode(crc)
        dataHdr += base64.b16decode(patchLen)
        dataHdr += base64.b16decode(magicNum)
        data = dataHdr + data

        w = io.open(output_bin_file, "wb")
        w.write(data)

        w.close()
        f.close()

        return output_bin_file
        
    def reverseEndianness(self, inputInt, intBool):
        temp = 0
        
        if intBool:
            temp = struct.pack('<I', inputInt)
            temp = struct.unpack('>I', temp)
        else:
            temp = struct.pack('<H', inputInt)
            temp = struct.unpack('>H', temp)
            
        temp = hex(int(str(temp).strip('(').strip(')').strip(',').strip('L'))).upper()
        temp = temp[2:].strip('L')
        
        if intBool and len(temp) < 8:
	    temp = '0'*(8-len(temp)) + temp
	if not intBool and len(temp) < 4:
	    temp = '0'*(4-len(temp)) + temp
	    
        return temp
        
def wrap_string_param(value_str):
    return '''"''' + value_str + '''"'''


if __name__ == "__main__":
    sys.exit(main())
