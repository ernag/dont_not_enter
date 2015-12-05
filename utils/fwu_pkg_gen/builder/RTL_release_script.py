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
import zipfile

boot1='boot1'
boot2='boot2'
app='app'
d_uart='d_uart'
skipbuild='skipbuild'
B_ver='B_ver'
pad='pad'


def main():
    RTL_release = RTL_release_script()
    return RTL_release.run(sys.argv)

class RTL_release_script(object):
    boot1_ver = ""
    boot2_ver = ""
    app_ver = ""
    odir = ""
    boot1_override = False
    argsList = []
    skip = False
    wiki = False
    uart = False

    def run(self, argv):
        retVal = 0
        parser = OptionParser()
        
        parser.add_option("--boot1_ver", dest="boot1_ver",
                          default=None,
                          help="Boot1 version")
        parser.add_option("--boot2_ver", dest="boot2_ver",
                          default=None,
                          help="Boot1 version")
        parser.add_option("--app_ver", dest="app_ver",
                          default=None,
                          help="Boot1 version")
        parser.add_option("--odir", dest="odir",
                          default=None,
                          help="directory where output files are placed")
        parser.add_option("--skip", dest="skip",
                          default=False,
                          help="skip building everything",
                          action="store_true")
        parser.add_option("--wiki", dest="wiki",
                          default=False,
                          help="generate just the wiki markup",
                          action="store_true")
        parser.add_option("--uart", dest="uart",
                          default=False,
                          help="false: build both UART and USB, true: build UART-only",
                          action="store_true")
        (opt, args) = parser.parse_args(argv)

        if opt.odir is None:
            print ("\nERROR: must supply output directory (--odir) for the RTL release script.\n")
            sys.exit()
            
        if opt.boot1_ver is None:
            self.boot1_override = True
        else:
            boot1_changes = raw_input("\nWARNING!!! You entered a boot1_ver. Have there been changes (do you still want a BOOT1 release)? (y/n): ")
            if "n" == boot1_changes:
                self.boot1_override = True
            elif "y" != boot1_changes:
                print "Invalid answer. Exiting..."
                sys.exit()
            
        if opt.boot2_ver is None:
            print ("\nERROR: must boot2 version (--boot2_ver) for the RTL release script.\n")
            sys.exit()
            
        if opt.app_ver is None:
            print ("\nERROR: must app version (--app_ver) for the RTL release script.\n")
            sys.exit()

        self.boot1_ver = opt.boot1_ver
        self.boot2_ver = opt.boot2_ver
        self.app_ver = opt.app_ver
        self.odir = opt.odir
        self.skip = opt.skip
        self.wiki = opt.wiki
        self.uart = opt.uart

        # Order matters because of skipbuilds
        if not self.wiki:
            if self.runOTC_UART() < 0:
                print "Failed to run OTC UART Build"
                return -1
            if self.runOTA_UART() < 0:
                print "Failed to run OTA UART Build"
                return -1
            if not self.uart:
                if self.runOTC_USB() < 0:
                    print "Failed to run OTC USB Build"
                    return -1
                if self.runOTA_USB() < 0:
                    print "Failed to run OTC USB Build"
                    return -1

        self.genWikiMarkup()

        if not self.wiki:
            self.dropFolder()
            self.FWUOTAFolder()
            self.zipFiles()

        return retVal

    def runOTC_UART(self):
        runOTC_UART_list = [\
        # B1 + B2 + APP
        {boot1:True, boot2:True, app:True, d_uart:True, pad:False, skipbuild:self.skip, B_ver:""},
        # B2 + APP
        {boot1:False, boot2:True, app:True, d_uart:True, pad:False, skipbuild:True, B_ver:""},
        # B1
        {boot1:True, boot2:False, app:False, d_uart:True, pad:False, skipbuild:True, B_ver:""},
        # B2
        {boot1:False, boot2:True, app:False, d_uart:True, pad:False, skipbuild:True, B_ver:""},
        # APP
        {boot1:False, boot2:False, app:True, d_uart:True, pad:False, skipbuild:True, B_ver:""}]

        return self.callBuilderOverList(runOTC_UART_list)

    def runOTA_UART(self):
        runOTA_UART_list = [\
        ### NON B ###
        # B1 + B2 + APP (non B)
        {boot1:True, boot2:True, app:True, d_uart:True, pad:True, skipbuild:True, B_ver:""},        
        # B2 + APP (non B)
        {boot1:False, boot2:True, app:True, d_uart:True, pad:True, skipbuild:True, B_ver:""},
        # B1 (non B)
        {boot1:True, boot2:False, app:False, d_uart:True, pad:True, skipbuild:True, B_ver:""},        
        # B2 (non B)
        {boot1:False, boot2:True, app:False, d_uart:True, pad:True, skipbuild:True, B_ver:""},        
        # APP (non B)
        {boot1:False, boot2:False, app:True, d_uart:True, pad:True, skipbuild:True, B_ver:""},
        ### B ###
        # B1 + B2 + APP (B)
        {boot1:True, boot2:True, app:True, d_uart:True, pad:True, skipbuild:True, B_ver:"-B"},
        # B2 + APP (B)
        {boot1:False, boot2:True, app:True, d_uart:True, pad:True, skipbuild:True, B_ver:"-B"},
        # B1 (B)
        {boot1:True, boot2:False, app:False, d_uart:True, pad:True, skipbuild:True, B_ver:"-B"},
        # B2 (B)
        {boot1:False, boot2:True, app:False, d_uart:True, pad:True, skipbuild:True, B_ver:"-B"},        
        # APP (B)
        {boot1:False, boot2:False, app:True, d_uart:True, pad:True, skipbuild:True, B_ver:"-B"}]
        
        return self.callBuilderOverList(runOTA_UART_list)
        
    def runOTC_USB(self):
        runOTC_USB_list = [\
        # B1 + B2 + APP
        {boot1:True, boot2:True, app:True, d_uart:False, pad:False, skipbuild:self.skip, B_ver:""},
        # B2 + APP
        {boot1:False, boot2:True, app:True, d_uart:False, pad:False, skipbuild:True, B_ver:""},
        # APP
        {boot1:False, boot2:False, app:True, d_uart:False, pad:False, skipbuild:True, B_ver:""}]

        return self.callBuilderOverList(runOTC_USB_list)

    def runOTA_USB(self):
        runOTA_USB_list = [\
        ### NON B ###
        # B1 + B2 + APP (non B)
        {boot1:True, boot2:True, app:True, d_uart:False, pad:True, skipbuild:True, B_ver:"" },
        # B2 + APP (non B}
        {boot1:False, boot2:True, app:True, d_uart:False, pad:True, skipbuild:True, B_ver:"" },
        # B1 (non B}
        {boot1:True, boot2:False, app:False, d_uart:False, pad:True, skipbuild:True, B_ver:"" },
        # B2 (non B}
        {boot1:False, boot2:True, app:False, d_uart:False, pad:True, skipbuild:True, B_ver:"" },
        # APP (non B}
        {boot1:False, boot2:False, app:True, d_uart:False, pad:True, skipbuild:True, B_ver:"" },
        ### B ###
        # B1 + B2 + APP (B}
        {boot1:True, boot2:True, app:True, d_uart:False, pad:True, skipbuild:True, B_ver:"-B" },
        # B2 + APP (B}
        {boot1:False, boot2:True, app:True, d_uart:False, pad:True, skipbuild:True, B_ver:"-B" },
        # B1 (B}
        {boot1:True, boot2:False, app:False, d_uart:False, pad:True, skipbuild:True, B_ver:"-B" },
        # B2 (B}
        {boot1:False, boot2:True, app:False, d_uart:False, pad:True, skipbuild:True, B_ver:"-B" },
        # APP (B}
        {boot1:False, boot2:False, app:True, d_uart:False, pad:True, skipbuild:True, B_ver:"-B" }]

        return self.callBuilderOverList(runOTA_USB_list)

    def callBuilderOverList(self, list_of_build_requests):
        for item in list_of_build_requests:
            res = self.callBuilder(boot1 = item[boot1],
                                   boot2 = item[boot2],
                                   app = item[app],
                                   d_uart = item[d_uart],
                                   pad = item[pad],
                                   skipbuild = item[skipbuild],
                                   B_ver = item[B_ver] )
            if res != 0:
                print "CALL FAILED"
                return -1

        return 0

    def callBuilder( self, boot1 = False, boot2 = False, app = False, d_uart = True, skipbuild = True, pad = False, B_ver = "" ):
        args = ["python", "builder.py", "--odir", self.odir, "--genpkg"]

        if boot1 and (not self.boot1_ver is None):
            args += ["--boot1", self.boot1_ver + B_ver]

        if boot2:
            args += ["--boot2", self.boot2_ver + B_ver]

        if app:
            if d_uart:
                args += ["--app", self.app_ver + "-a" + B_ver]
            else:
                args += ["--app", self.app_ver + "-s" + B_ver]

        if skipbuild:
            args.append("--skipbuild")

        if pad:
            args.append("--pad")

        if d_uart:
            args.append("--d_uart")
        else:
            args.append("--d_usb")

        
        # If BOOT1 was the only image in the package
        # and there is a boot1_override, don't run builder
        if (self.boot1_override and not (boot2 or app)):
            print "Skipping BOOT1 ONLY build..."
            return 0
        elif args in self.argsList:
            print "Already built. Skipping..."
            return 0
        else:
            self.argsList.append(args)
            print "Running args:", args
            proc = subprocess.Popen(args)
            res = proc.wait()
            print " RESULT: {}".format(res)
            return res

    def genWikiMarkup(self):
        w = open(self.odir + "\\WIKI_MARKUP.TXT", "w")
        
        output = 'h1.{color:red}<DATE>{color} Weekly RTL Release\n'
        output += '*SHA ID: {color:red}<copy/paste SHA ID from Git log - TAG THIS CHECK-IN W/ RTL_release>{color}*\n'
        output += 'h2.Packages\n'
        output += '[^APP_%s_d_uart.pkg]\n' % (self.app_ver + "-a")
        output += '[^BOOT2_%s_d_uart.pkg]\n' % self.boot2_ver
        if not self.boot1_override:
            output += '[^BOOT1_%s_d_uart.pkg]\n' % self.boot1_ver
        output += '[^BOOT2_%s_APP_%s_d_uart.pkg]\n' % (self.boot2_ver, (self.app_ver + "-a"))
        if not self.boot1_ver is None:
            output += '[^BOOT1_%s_BOOT2_%s_APP_%s_d_uart.pkg]\n' % (self.boot1_ver, self.boot2_ver, (self.app_ver + "-a"))
        output += '[^packages_%s.zip]\n' % self.app_ver
        output += '||image||changed?||version||\n'

        if not self.boot1_override:
            output += '|Boot1|yes|%s|\n' % self.boot1_ver
        else:
            output += '|Boot1|no| |\n'
        
        output += '|Boot2|yes|%s|\n' % self.boot2_ver
        output += '|App UART|yes|%s|\n' % (self.app_ver + "-a")

        if not self.uart:
            output += '|App USB|yes|%s|\n' % (self.app_ver + "-s")

        output += 'h2.App_%s_d_UART - UART\n' % (self.app_ver + "-a")
        output += 'h3.Release Notes\n'
        output += '* New features\n'
        output += '** {color:red}Feature 1{color}\n'
        output += '* Bug fixes\n'
        output += '** {color:red}Bug fix 1{color}\n'
        output += '* Known bugs\n'
        output += '** {color:red}Enhancement 1{color}\n'
        output += '* Known bugs\n'
        output += '** {color:red}Known bug 1{color}\n'
        output += '* Pending features\n'
        output += '** {color:red}Pending 1{color}\n'
        output += '\n'
        output += 'h3.Package Release\n'
        output += '[^APP_%s_d_uart.pkg]\n' % (self.app_ver + "-a")

        if not self.uart:
            output += 'h2.App_%s_d_USB - USB\n' % (self.app_ver + "-s")
            output += 'h3.Release Notes\n'
            output += '* {color:red}Updates{color}\n'
            output += '\n'
            output += 'h3.Package Release\n'
            output += '[^APP_%s_d_usb.pkg]\n' % (self.app_ver + "-s")
        
        if not self.boot1_override:
            output += 'h2.Boot1_%s - Boot1 Release\n' % self.boot1_ver
            output += 'h3.Release Notes\n'
            output += '* {color:red}UPDATES{color}\n'
            output += '\n'
            output += 'h3.Package Release\n'
            output += '[^BOOT1_%s_d_uart.pkg]\n' % self.boot1_ver
        
        output += 'h2.Boot2_%s - Boot2 Release\n' % self.boot2_ver
        output += 'h3.Release Notes\n'
        output += '* {color:red}UPDATES{color}\n'
        output += '\n'
        output += 'h3.Package Release\n'
        output += '[^BOOT2_%s_d_uart.pkg]\n' % self.boot2_ver
        output += 'h2.Package Components - .bin and .xMAP\n'
        output += '[^package_components_%s.zip]\n' % self.app_ver

        w.write(output.encode('utf-8'))

        w.close()

    def zipFiles(self):
        pkg_zipList = []

        if not self.boot1_override:
            pkg_zipList.append("BOOT1_%s_d_uart.pkg" % self.boot1_ver)
            pkg_zipList.append("BOOT1_%s_PADDED_d_uart.pkg" % self.boot1_ver)
            pkg_zipList.append("BOOT1_%s-B_PADDED_d_uart.pkg" % self.boot1_ver)

        pkg_zipList.append("BOOT2_%s_d_uart.pkg" % self.boot2_ver)
        pkg_zipList.append("BOOT2_%s_PADDED_d_uart.pkg" % self.boot2_ver)
        pkg_zipList.append("BOOT2_%s-B_PADDED_d_uart.pkg" % self.boot2_ver)

        pkg_zipList.append("APP_%s_d_uart.pkg" % (self.app_ver + "-a"))
        pkg_zipList.append("APP_%s_PADDED_d_uart.pkg" % (self.app_ver + "-a"))
        pkg_zipList.append("APP_%s-B_PADDED_d_uart.pkg" % (self.app_ver + "-a"))

        if not self.uart:
            pkg_zipList.append("APP_%s_d_usb.pkg" % (self.app_ver + "-s"))
            pkg_zipList.append("APP_%s_PADDED_d_usb.pkg" % (self.app_ver + "-s"))
            pkg_zipList.append("APP_%s-B_PADDED_d_usb.pkg" % (self.app_ver + "-s"))

        if not self.boot1_ver is None:
            pkg_zipList.append("BOOT1_%s_BOOT2_%s_APP_%s_d_uart.pkg" % (self.boot1_ver, self.boot2_ver, (self.app_ver + "-a")))
            pkg_zipList.append("BOOT1_%s_BOOT2_%s_APP_%s_PADDED_d_uart.pkg" % (self.boot1_ver, self.boot2_ver, (self.app_ver + "-a")))
            pkg_zipList.append("BOOT1_%s-B_BOOT2_%s-B_APP_%s-B_PADDED_d_uart.pkg" % (self.boot1_ver, self.boot2_ver, (self.app_ver + "-a")))

            if not self.uart:
                pkg_zipList.append("BOOT1_%s_BOOT2_%s_APP_%s_d_usb.pkg" % (self.boot1_ver, self.boot2_ver, (self.app_ver + "-s")))
                pkg_zipList.append("BOOT1_%s_BOOT2_%s_APP_%s_PADDED_d_usb.pkg" % (self.boot1_ver, self.boot2_ver, (self.app_ver + "-s")))
                pkg_zipList.append("BOOT1_%s-B_BOOT2_%s-B_APP_%s-B_PADDED_d_usb.pkg" % (self.boot1_ver, self.boot2_ver, (self.app_ver + "-s")))

        pkg_zipList.append("BOOT2_%s_APP_%s_d_uart.pkg" % (self.boot2_ver, (self.app_ver + "-a")))
        pkg_zipList.append("BOOT2_%s_APP_%s_PADDED_d_uart.pkg" % (self.boot2_ver, (self.app_ver + "-a")))
        pkg_zipList.append("BOOT2_%s-B_APP_%s-B_PADDED_d_uart.pkg" % (self.boot2_ver, (self.app_ver + "-a")))

        if not self.uart:
            pkg_zipList.append("BOOT2_%s_APP_%s_d_usb.pkg" % (self.boot2_ver, (self.app_ver + "-s")))
            pkg_zipList.append("BOOT2_%s_APP_%s_PADDED_d_usb.pkg" % (self.boot2_ver, (self.app_ver + "-s")))
            pkg_zipList.append("BOOT2_%s-B_APP_%s-B_PADDED_d_usb.pkg" % (self.boot2_ver, (self.app_ver + "-s")))
        
        with zipfile.ZipFile(self.odir + "\\dropFolder\\packages_" + self.app_ver + ".zip", 'w') as pkg_zip:
            for pkgfile in pkg_zipList:
                pkg_zip.write(self.odir + "\\" + pkgfile, pkgfile)


        pkg_comp_zipList = []
        if not self.boot1_override:
            pkg_comp_zipList.append("BOOT1_%s_d_uart.bin" % self.boot1_ver)
            pkg_comp_zipList.append("BOOT1_%s_d_uart.xMap" % self.boot1_ver)
        
        pkg_comp_zipList.append("BOOT2A_%s_d_uart.bin" % self.boot2_ver)
        pkg_comp_zipList.append("BOOT2A_%s_d_uart.xMap" % self.boot2_ver)
        pkg_comp_zipList.append("BOOT2B_%s_d_uart.bin" % self.boot2_ver)
        pkg_comp_zipList.append("BOOT2B_%s_d_uart.xMap" % self.boot2_ver)
        pkg_comp_zipList.append("APP_%s_d_uart.bin" % (self.app_ver + "-a"))
        pkg_comp_zipList.append("APP_%s_d_uart.xMap" % (self.app_ver + "-a"))
        if not self.uart:
            pkg_comp_zipList.append("APP_%s_d_usb.bin" % (self.app_ver + "-s"))
            pkg_comp_zipList.append("APP_%s_d_usb.xMap" % (self.app_ver + "-s"))

        with zipfile.ZipFile(self.odir + "\\dropFolder\\package_components_" + self.app_ver + ".zip", 'w') as pkg_comp_zip:
            for pkg_comp_file in pkg_comp_zipList:
                pkg_comp_zip.write(self.odir + "\\" + pkg_comp_file, pkg_comp_file)

    def dropFolder(self):
        os.mkdir(self.odir + "\\dropFolder")
        shutil.copyfile(self.odir + "\\" + ("APP_%s_d_uart.pkg" % (self.app_ver + "-a")), self.odir + "\\dropFolder\\" + ("APP_%s_d_uart.pkg" % (self.app_ver + "-a")))
        if not self.uart:
            shutil.copyfile(self.odir + "\\" + ("APP_%s_d_usb.pkg" % (self.app_ver + "-s")), self.odir + "\\dropFolder\\" + ("APP_%s_d_usb.pkg" % (self.app_ver + "-s")))
        shutil.copyfile(self.odir + "\\" + ("BOOT2_%s_d_uart.pkg" % self.boot2_ver), self.odir + "\\dropFolder\\" + ("BOOT2_%s_d_uart.pkg" % self.boot2_ver))
        shutil.copyfile(self.odir + "\\" + ("BOOT2_%s_APP_%s_d_uart.pkg" % (self.boot2_ver, (self.app_ver + "-a"))), self.odir + "\\dropFolder\\" + ("BOOT2_%s_APP_%s_d_uart.pkg" % (self.boot2_ver, (self.app_ver + "-a"))))
        if not self.boot1_ver is None:
            if not self.boot1_override:
                shutil.copyfile(self.odir + "\\" + ("BOOT1_%s_d_uart.pkg" % self.boot1_ver), self.odir + "\\dropFolder\\" + ("BOOT1_%s_d_uart.pkg" % self.boot1_ver))
            shutil.copyfile(self.odir + "\\" + ("BOOT1_%s_BOOT2_%s_APP_%s_d_uart.pkg" % (self.boot1_ver, self.boot2_ver, (self.app_ver + "-a"))), self.odir + "\\dropFolder\\" + ("BOOT1_%s_BOOT2_%s_APP_%s_d_uart.pkg" % (self.boot1_ver, self.boot2_ver, (self.app_ver + "-a"))))

    def FWUOTAFolder(self):
        os.mkdir(self.odir + "\\FWUOTA_Folder")
        shutil.copyfile(self.odir + "\\" + ("BOOT2_%s_APP_%s_PADDED_d_uart.pkg" % (self.boot2_ver, (self.app_ver + "-a"))), self.odir + "\\FWUOTA_Folder\\" + ("BOOT2_%s_APP_%s_PADDED_d_uart.pkg" % (self.boot2_ver, (self.app_ver + "-a"))))
        shutil.copyfile(self.odir + "\\" + ("BOOT2_%s-B_APP_%s-B_PADDED_d_uart.pkg" % (self.boot2_ver, (self.app_ver + "-a"))), self.odir + "\\FWUOTA_Folder\\" + ("BOOT2_%s-B_APP_%s-B_PADDED_d_uart.pkg" % (self.boot2_ver, (self.app_ver + "-a"))))
        if not self.boot1_ver is None:
            shutil.copyfile(self.odir + "\\" + ("BOOT1_%s_BOOT2_%s_APP_%s_PADDED_d_uart.pkg" % (self.boot1_ver, self.boot2_ver, (self.app_ver + "-a"))), self.odir + "\\FWUOTA_Folder\\" + ("BOOT1_%s_BOOT2_%s_APP_%s_PADDED_d_uart.pkg" % (self.boot1_ver, self.boot2_ver, (self.app_ver + "-a"))))
            shutil.copyfile(self.odir + "\\" + ("BOOT1_%s-B_BOOT2_%s-B_APP_%s-B_PADDED_d_uart.pkg" % (self.boot1_ver, self.boot2_ver, (self.app_ver + "-a"))), self.odir + "\\FWUOTA_Folder\\" + ("BOOT1_%s-B_BOOT2_%s-B_APP_%s-B_PADDED_d_uart.pkg" % (self.boot1_ver, self.boot2_ver, (self.app_ver + "-a"))))
        
if __name__ == "__main__":
    main()
    










        
