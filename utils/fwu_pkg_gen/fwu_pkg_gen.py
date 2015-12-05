from optparse import OptionParser
import io, sys
import os
import tlv

def main():
    generator = FwuPackageGenerator()
    return generator.run(sys.argv)

def contains_options_for_file_generation(options):
    return (options.out_file or
            options.boot1_img or options.boot1_ver or
            options.boot2_pic_img or options.boot2_pic_ver or
            options.boot2a_img or options.boot2a_ver or
            options.boot2b_img or options.boot2b_ver or
            options.app_img or options.app_ver or
            options.basepatch_bin or options.blepatch_bin)

class FwuPackageGenerator(object):
    def run(self, argv):
        print ("FWU Package Generator Running...")
        parser = OptionParser()
        self.setupOptions(parser)

        (options, args) = parser.parse_args(argv)

        print options

        debug = False
        if options.verbose:
            debug = True

        if options.in_file and (contains_options_for_file_generation(options)):
            print("INPUT ERROR: cannot take both input and output files")
            return

        if options.outbin and not options.in_file:
            print "ERROR: Must input a .pkg file (-i) to use outbin"
            return

        if options.in_file:
            self.parse_and_print(options.in_file, options.outbin, verbose=options.verbose)
            return

        if (contains_options_for_file_generation(options)):
            if not options.out_file:
                print("INPUT ERROR: must have a specified output file when creating a new package")
                return
            if options.boot1_img and not options.boot1_ver:
                print("INPUT ERROR: Boot1 image must specify version (format: x.y-tag)")
                return
            if options.boot1_ver and not options.boot1_img:
                print("INPUT ERROR: Cannot specify boot1 version without supplying boot1 img path.")
                return
            if options.boot2_pic_img and not options.boot2_pic_ver:
                print("INPUT ERROR: Boot2 PIC image must specify version (format: x.y-tag)")
                return
            if options.boot2_pic_ver and not options.boot2_pic_img:
                print("INPUT ERROR: Cannot specify boot2 PIC version without supplying boot2 PIC img path.")
                return
            if options.boot2a_img and not options.boot2a_ver:
                print("INPUT ERROR: Boot2A image must specify version (format: x.y-tag)")
                return
            if options.boot2a_ver and not options.boot2a_img:
                print("INPUT ERROR: Cannot specify boot2A version without supplying boot2A img path.")
                return
            if options.boot2b_img and not options.boot2b_ver:
                print("INPUT ERROR: Boot2B image must specify version (format: x.y-tag)")
                return
            if options.boot2b_ver and not options.boot2b_img:
                print("INPUT ERROR: Cannot specify boot2B version without supplying boot2B img path.")
                return
            if options.app_img and not options.app_ver:
                print("INPUT ERROR: App image must specify version (format: x.y-tag)")
                return
            if options.app_ver and not options.app_img:
                print("INPUT ERROR: Cannot specify app version without supplying app img path.")
                return
            if options.boot1_img:
                if not os.access(options.boot1_img, os.F_OK):
                    print("INPUT ERROR: Boot1 image not accessible.")
            if options.boot2_pic_img:
                if not os.access(options.boot2_pic_img, os.F_OK):
                    print("INPUT ERROR: Boot2 PIC image not accessible.")
            if options.boot2a_img:
                if not os.access(options.boot2a_img, os.F_OK):
                    print("INPUT ERROR: Boot2A image not accessible.")
            if options.boot2b_img:
                if not os.access(options.boot2b_img, os.F_OK):
                    print("INPUT ERROR: Boot2B image not accessible.")
            if options.app_img:
                if not os.access(options.app_img, os.F_OK):
                    print("INPUT ERROR: App image not accessible.")
            if options.basepatch_bin:
                if not os.access(options.basepatch_bin, os.F_OK):
                    print("INPUT ERROR: BASEPATCH binary not accessible.")
            if options.blepatch_bin:
                if not os.access(options.blepatch_bin, os.F_OK):
                    print("INPUT ERROR: BLEPATCH binary not accessible.")
            self.generate_package(options)
            return

        print("ERROR: unexpected commandline options set")
        return

    def parse_and_print(self, file_loc, outbin, verbose=False):

        fh = io.FileIO(file_loc, "r")
        serialized_string = fh.readall()
        fh.close()

        contents_loc = 0
        tlv_obj = tlv.TLV()
        rawdataobjects = []

        tlv_obj = tlv.TLV.from_string(serialized_string, contents_loc)
        rawdataoffset = tlv_obj._raw_payload_offset
        if rawdataoffset <= 0:
            rawdataoffset = len(serialized_string)

        while (contents_loc < rawdataoffset):
            tlv_obj = tlv.TLV.from_string(serialized_string, contents_loc)
            if tlv_obj._tlv_type == tlv.TLV._TLV_TYPE_BINARY_IMAGE:
                
                bin_name = tlv.BinaryImageTLV.binary_image_type_to_str(tlv_obj.image_type) + '_' +\
                           str(tlv_obj.version_tag) + "_v" +\
                           str(tlv_obj.major_version) + '.' + str(tlv_obj.minor_version) + ".bin"
                
                rawdataobjects.append([tlv_obj.image_type,
                                       tlv_obj.raw_image_offset,
                                       tlv_obj.raw_image_size,
                                       bin_name])

            tlv_obj.print_summary(verbose=verbose)
            contents_loc += tlv_obj.size
        print "Raw Data Offset: " + str(rawdataoffset)

        if verbose or outbin:
            for dataobj in rawdataobjects:
                type = dataobj[0]
                offset = rawdataoffset + dataobj[1]
                size = dataobj[2]
                type_str = tlv.BinaryImageTLV.binary_image_type_to_str(type)
                if verbose:
                    print "Payload: " + type_str + ": " + str(serialized_string[offset:offset + size])
                if outbin:
                    bin_name = dataobj[3]
                    print "\nOutputting to bin file:", bin_name, "\nSize:", size
                    try:
                        ob_fh = open(bin_name, 'wb+')
                        ob_fh.write(serialized_string[offset:offset + size])
                        ob_fh.close()
                    except:
                        print "ERROR: in file I/O for file:", bin_name
                        sys.exit()

    def generate_package(self, options):
        """

        :param options:
        """

        non_info_serialized = str()
        binary_payload = str()
        binary_payload_basepatch = str()
        binary_payload_blepatch = str()

        #FWU Dep on Boot1
        if options.dep_boot1:
            [major, minor] = map(int, options.dep_boot1.split(".", 1))
            tlv_dep_boot1 = tlv.MinimumVersionDependencyTLV(tlv.MinimumVersionDependencyTLV.IMAGE_TYPE_BOOT1,
                                                         major, minor)
            non_info_serialized += tlv_dep_boot1.to_string()

        #FWU Dep on Boot2
        if options.dep_boot2:
            [major, minor] = map(int, options.dep_boot2.split(".", 1))
            tlv_dep_boot2 = tlv.MinimumVersionDependencyTLV(tlv.MinimumVersionDependencyTLV.IMAGE_TYPE_BOOT2,
                                                         major, minor)
            non_info_serialized += tlv_dep_boot2.to_string()

        #FWU Dep on app
        if options.dep_app:
            [major, minor] = map(int, options.dep_app.split(".", 1))
            tlv_dep_app = tlv.MinimumVersionDependencyTLV(tlv.MinimumVersionDependencyTLV.IMAGE_TYPE_APP,
                                                       major, minor)
            non_info_serialized += tlv_dep_app.to_string()


        contents = 0

        #FWU Boot1 Image
        if options.boot1_img and options.boot1_ver:
            [major, minor, tag] = tlv.BinaryImageTLV.parse_version_string(options.boot1_ver)
            tlv_boot1_img = tlv.BinaryImageTLV(tlv.BinaryImageTLV.BINARY_IMAGE_TYPE_BOOT1,
                                            major, minor, tag, raw_image_loc=options.boot1_img,
                                            raw_image_offset=len(binary_payload))
            non_info_serialized += tlv_boot1_img.to_string()
            contents |= tlv.FwuPkgInfoTLV.CONTENTS_MASK_BOOT1
            fh = io.FileIO(options.boot1_img, "r")
            binary_payload += fh.readall()
            fh.close()

        #FWU Boot2 PIC Image
        if options.boot2_pic_img and options.boot2_pic_ver:
            [major, minor, tag] = tlv.BinaryImageTLV.parse_version_string(options.boot2_pic_ver)
            tlv_boot2_pic_img = tlv.BinaryImageTLV(tlv.BinaryImageTLV.BINARY_IMAGE_TYPE_BOOT2_PIC,
                                            major, minor, tag, raw_image_loc=options.boot2_pic_img,
                                            raw_image_offset=len(binary_payload))
            non_info_serialized += tlv_boot2_pic_img.to_string()
            contents |= tlv.FwuPkgInfoTLV.CONTENTS_MASK_BOOT2_PIC
            fh = io.FileIO(options.boot2_pic_img, "r")
            binary_payload += fh.readall()
            fh.close()

        #FWU App Image
        if options.app_img and options.app_ver:
            [major, minor, tag] = tlv.BinaryImageTLV.parse_version_string(options.app_ver)
            tlv_app_img = tlv.BinaryImageTLV(tlv.BinaryImageTLV.BINARY_IMAGE_TYPE_APP,
                                            major, minor, tag, raw_image_loc=options.app_img,
                                            raw_image_offset=len(binary_payload))
            non_info_serialized += tlv_app_img.to_string()
            contents |= tlv.FwuPkgInfoTLV.CONTENTS_MASK_APP
            fh = io.FileIO(options.app_img, "r")
            binary_payload += fh.readall()
            fh.close()

        #FWU Boot2A Image
        if options.boot2a_img and options.boot2a_ver:
            [major, minor, tag] = tlv.BinaryImageTLV.parse_version_string(options.boot2a_ver)
            tlv_boot2a_img = tlv.BinaryImageTLV(tlv.BinaryImageTLV.BINARY_IMAGE_TYPE_BOOT2A,
                                               major, minor, tag, raw_image_loc=options.boot2a_img,
                                               raw_image_offset=len(binary_payload))
            non_info_serialized += tlv_boot2a_img.to_string()
            contents |= tlv.FwuPkgInfoTLV.CONTENTS_MASK_BOOT2A
            fh = io.FileIO(options.boot2a_img, "r")
            binary_payload += fh.readall()
            fh.close()

        #FWU Boot2B Image
        if options.boot2b_img and options.boot2b_ver:
            [major, minor, tag] = tlv.BinaryImageTLV.parse_version_string(options.boot2b_ver)
            tlv_boot2b_img = tlv.BinaryImageTLV(tlv.BinaryImageTLV.BINARY_IMAGE_TYPE_BOOT2B,
                                               major, minor, tag, raw_image_loc=options.boot2b_img,
                                               raw_image_offset=len(binary_payload))
            non_info_serialized += tlv_boot2b_img.to_string()
            contents |= tlv.FwuPkgInfoTLV.CONTENTS_MASK_BOOT2B
            fh = io.FileIO(options.boot2b_img, "r")
            binary_payload += fh.readall()
            fh.close()

        #FWU Patch Binary
        if options.basepatch_bin:
            tlv_basepatch_bin = tlv.PatchTLV(tlv.PatchTLV.PATCH_TYPE_BASEPATCH,
                                         raw_patch_loc=options.basepatch_bin,
                                         raw_patch_offset=len(binary_payload))
            non_info_serialized += tlv_basepatch_bin.to_string()
            contents |= tlv.FwuPkgInfoTLV.CONTENTS_MASK_BASEPATCH
            fh = io.open(options.basepatch_bin, "rb")
            binary_payload_basepatch = fh.read()
            fh.close()

        if options.blepatch_bin:
            tlv_blepatch_bin = tlv.PatchTLV(tlv.PatchTLV.PATCH_TYPE_LOWENERGY,
                                         raw_patch_loc=options.blepatch_bin,
                                         raw_patch_offset=len(binary_payload + binary_payload_basepatch))
            non_info_serialized += tlv_blepatch_bin.to_string()
            contents |= tlv.FwuPkgInfoTLV.CONTENTS_MASK_LOWENERGY
            fh = io.open(options.blepatch_bin, "rb")
            binary_payload_blepatch = fh.read()
            fh.close()

        #FWU Package Info
        tlv_pkg_info = tlv.FwuPkgInfoTLV(options.title, contents)
        tlv_pkg_info.raw_payload_offset = tlv_pkg_info.size + len(non_info_serialized)
        tlv_pkg_info.update_size_crc_signature(non_info_serialized + binary_payload + binary_payload_basepatch + binary_payload_blepatch)
        info_serialized = tlv_pkg_info.to_string()

        if options.pad:
            print "Padding array 1500 bytes"
            binary_payload += str(bytearray([190]*1500))

        fh = io.FileIO(options.out_file, "wb")
        fh.write(info_serialized + non_info_serialized + binary_payload + binary_payload_basepatch + binary_payload_blepatch)
        fh.close()
        

    def setupOptions(self, parser):
        parser.add_option("-1", "--boot1-img",
                          default=None,
                          help="The boot1 binary image file location")
        parser.add_option("--boot1-ver",
                          default=None,
                          help="The boot1 binary version. Format: \"x.y-tag\" .")
        parser.add_option("--boot2-pic-img",
                          default=None,
                          help="The boot2 PIC binary image file location")
        parser.add_option("--boot2-pic-ver",
                          default=None,
                          help="The boot2 PIC binary version. Format: \"x.y-tag\" .")
        parser.add_option("-a", "--boot2a-img",
                          default=None,
                          help="The boot2A binary image file location")
        parser.add_option("--boot2a-ver",
                          default=None,
                          help="The boot2A binary version. Format: \"x.y-tag\" .")
        parser.add_option("-b", "--boot2b-img",
                          default=None,
                          help="The boot2B binary image file location")
        parser.add_option("--boot2b-ver",
                          default=None,
                          help="The boot2B binary version. Format: \"x.y-tag\" .")
        parser.add_option("-m", "--app-img",
                          default=None,
                          help="The app binary image file location")
        parser.add_option("--app-ver",
                          default=None,
                          help="The app binary version. Format: \"x.y-tag\" .")
        parser.add_option("--basepatch-bin",
                          default=None,
                          help="The file path to the BASEPATCH binary file.")
        parser.add_option("--blepatch-bin",
                          default=None,
                          help="The file path to the BLEPATCH binary file.")
        parser.add_option("--outbin",
                          default=False, action="store_true",
                          help="Output bin file(s) corresponding to .pkg file.")
        parser.add_option("--dep-boot1",
                          default=None,
                          help="Set dependency on boot1 image. Format: \"x.y-tag\" .")
        parser.add_option("--dep-boot2",
                          default=None,
                          help="Set dependency on boot2 image. Format: \"x.y-tag\" .")
        parser.add_option("--dep-app",
                          default=None,
                          help="Set dependency on application image. Format: \"x.y-tag\" .")

        parser.add_option("-t", "--title",
                          default=None,
                          help="Title of the firmware update package. [Optional]")
        parser.add_option("-o", "--out-file",
                          default=None,
                          help="the output file, "
                               "[default: %default]")
        parser.add_option("--pad", dest="pad",
                          default=False,
                          help="appends 1500 bytes to the end of the pkg. Needed for devices that have Atheros bug "
                               "that looses the payload of the final TCP packet if it is a FIN",
                          action="store_true")

        parser.add_option("-i", "--in-file",
                          default=None,
                          help="input file to parse and output summary of.")

        parser.add_option("-v", action="store_true", dest="verbose", help="displays contents of included images.")
        parser.add_option("-q", action="store_false", dest="verbose")



if __name__ == "__main__":
    main()
