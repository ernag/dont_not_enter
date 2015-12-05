import io
import binascii


def le_hexstr_to_int16(source, index):
    res = 0
    res += ord(source[index])
    res += ord(source[index + 1]) << 8
    return res


def le_hexstr_to_int32(source, index):
    res = 0
    res += ord(source[index])
    res += ord(source[index + 1]) << 8
    res += ord(source[index + 2]) << 16
    res += ord(source[index + 3]) << 24
    return res


def int32_to_le_hexstr(num):
    res = ""
    res += chr(num & 0xFF)
    res += chr((num >> 8) & 0xFF)
    res += chr((num >> 16) & 0xFF)
    res += chr((num >> 24) & 0xFF)
    return res


def int16_to_le_hexstr(num):
    res = ""
    res += chr(num & 0xFF)
    res += chr((num >> 8) & 0xFF)
    return res


def generate_package_signature(secret_key, package_crc):
    return 0


class TLV(object):
    _tlv_header_size = 6
    _tlv_payload_size = 0

    _TLV_TYPE_FWU_PKG_INFO = 1
    _TLV_TYPE_MINIMUM_VERSRION_DEPENDENCY = 10
    _TLV_TYPE_BINARY_IMAGE = 11
    _TLV_TYPE_PATCH = 21

    _tlv_type = -1
    # length = @property

    def __init__(self):
        return

    # This length is to be overloaded
    @property
    def size(self):
        return self._tlv_header_size + self._tlv_payload_size

    def to_string(self):
        return self._header_to_string() + self._payload_to_string()

    def _header_to_string(self):
        res = str()

        # Type (uint16)
        res += int16_to_le_hexstr(self._tlv_type)

        # Length (uint32)
        res += int32_to_le_hexstr(self.size)

        return res

    def _payload_to_string(self):
        return str()

    @classmethod
    def from_string(self, source, start_loc=0):
        source_loc = start_loc
        tlv_type = le_hexstr_to_int16(source, source_loc)
        source_loc += 2

        if tlv_type == TLV._TLV_TYPE_FWU_PKG_INFO:
            res = FwuPkgInfoTLV()
        elif tlv_type == TLV._TLV_TYPE_MINIMUM_VERSRION_DEPENDENCY:
            res = MinimumVersionDependencyTLV()
        elif tlv_type == TLV._TLV_TYPE_BINARY_IMAGE:
            res = BinaryImageTLV()
        else:
            return None

        res._tlv_import_size = le_hexstr_to_int32(source, source_loc)
        source_loc += 4

        res._payload_from_string(source, source_loc)
        return res

    def _payload_from_string(self, source=None, start_loc=0):
        return ""

    def print_summary(self, verbose=False):
        self.print_header_summary(verbose)
        self.print_payload_summary(verbose)

    def print_header_summary(self, verbose=False):
        type_str = str(self._tlv_type)
        if self._tlv_type == TLV._TLV_TYPE_FWU_PKG_INFO:
            type_str += " (FWU Package Info)"
        if self._tlv_type == TLV._TLV_TYPE_MINIMUM_VERSRION_DEPENDENCY:
            type_str += " (FWU Minimum Version Dependency)"
        if self._tlv_type == TLV._TLV_TYPE_BINARY_IMAGE:
            type_str += " (FWU Binary Image)"
        print("TLV Type: " + type_str)

    def print_payload_summary(self, verbose=False):
        print("")


class FwuPkgInfoTLV(TLV):
    _tlv_payload_size = 66
    _tlv_import_size = -1
    _tlv_type = TLV._TLV_TYPE_FWU_PKG_INFO

    CONTENTS_MASK_BOOT1 =   0b00000001
    CONTENTS_MASK_BOOT2_PIC = 0b00000010
    CONTENTS_MASK_APP =     0b00000100
    CONTENTS_MASK_CONFIG =  0b00001000
    CONTENTS_MASK_BOOT2A =  0b00010000
    CONTENTS_MASK_BOOT2B =  0b00100000
    CONTENTS_MASK_BASEPATCH = 0b01000000
    CONTENTS_MASK_LOWENERGY = 0b10000000

    def __init__(self, title="", contents=0, size=0, CRC=None, signature=None, secret_key=0, raw_payload_offset=0):
        TLV.__init__(self)
        self._pkg_title = title.encode("ascii") if title is not None else None
        self._pkg_size = size
        self._pkg_crc = CRC
        self._pkg_signature = signature
        self._pkg_contents = contents
        self._raw_payload_offset = raw_payload_offset
        self._secret_key = secret_key  # Note, this is only used for the package signature calculation

    @property
    def pkg_title(self):
        return self._pkg_title

    @property
    def pkg_crc(self):
        return self._pkg_crc

    @property
    def pkg_signature(self):
        return self._pkg_signature

    @property
    def pkg_contents(self):
        return self._pkg_contents

    @pkg_contents.setter
    def pkg_contents(self, value):
        self._pkg_contents = value

    @property
    def raw_payload_offset(self):
        return self._raw_payload_offset

    @raw_payload_offset.setter
    def raw_payload_offset(self, value):
        self._raw_payload_offset = value

    @property
    def pkg_size(self):
        return self._pkg_size

    def set_package_contents(self, content_mask):
        self._pkg_contents = content_mask

    def update_size_crc_signature(self, package_serialized_string, secret_key=0):
        if secret_key == 0:
            secret_key = self._secret_key

        self._pkg_size = self.size + len(package_serialized_string)

        dumby_pkg_info = FwuPkgInfoTLV(title=self._pkg_title,
                                       size=self._pkg_size,
                                       CRC=0, signature=0,
                                       contents=self._pkg_contents,
                                       raw_payload_offset=self._raw_payload_offset)
        dumby_pkg_info_crc = binascii.crc32(dumby_pkg_info.to_string())
        package_crc = binascii.crc32(package_serialized_string, dumby_pkg_info_crc)
        self._pkg_crc = package_crc & 0xFFFFFFFF

        self._pkg_signature = generate_package_signature(self._secret_key, self._pkg_crc)

    def _payload_to_string(self):
        res = str()

        # Package Title (char[48])
        if self._pkg_title:
            res += self._pkg_title[:48].ljust(48, chr(0x0)) # make sure length <= 48 and if less then pad right with 0x00
        else:
            res += chr(0x00)*48

        # Package Size (uint32)
        res += int32_to_le_hexstr(self._pkg_size)

        # Package CRC (uint32)
        res += int32_to_le_hexstr(self._pkg_crc)

        # Package Signature (uint32)
        res += int32_to_le_hexstr(self._pkg_signature)

        # Package Contents (uint16)
        res += int16_to_le_hexstr(self._pkg_contents)

        # Raw Payload Start (uint32)
        res += int32_to_le_hexstr(self._raw_payload_offset)
        return res

    def _payload_from_string(self, source=None, start_loc=0):
        source_loc = start_loc

        # Package Title (char[48])
        self._pkg_title = source[source_loc:source_loc + 48].replace(chr(0), " ").rstrip()
        source_loc += 48

        # Package Size (uint32)
        self._import_pkg_size = le_hexstr_to_int32(source, source_loc)
        self._pkg_size = self._import_pkg_size
        source_loc += 4

        # Package CRC (uint32)
        self._import_pkg_crc = le_hexstr_to_int32(source, source_loc)
        self._pkg_crc = self._import_pkg_crc
        source_loc += 4

        # Package Signature (uint32)
        self._import_pkg_signature = le_hexstr_to_int32(source, source_loc)
        self._pkg_signature = self._import_pkg_signature
        source_loc += 4

        # Package Contents (uint16)
        self._pkg_contents = le_hexstr_to_int16(source, source_loc)
        source_loc += 2

        # Raw Payload Offset (uint32)
        self._raw_payload_offset = le_hexstr_to_int32(source, source_loc)
        source_loc += 4

    def validate_signature(self):
        return False

    def print_payload_summary(self, verbose=False):
        print("  Pkg Title: " + self.pkg_title)
        print("  Pkg Size: " + str(self.pkg_size))
        print("  Pkg CRC: " + hex(self.pkg_crc))
        print("  Pkg Signature: " + hex(self.pkg_signature))
        contents = str("")
        if self.pkg_contents & FwuPkgInfoTLV.CONTENTS_MASK_BOOT1:
            contents += "BOOT1"
        if self.pkg_contents & FwuPkgInfoTLV.CONTENTS_MASK_BOOT2_PIC:
            if contents != "":
                contents += ", "
            contents += "BOOT2-PIC"
        if self.pkg_contents & FwuPkgInfoTLV.CONTENTS_MASK_BOOT2A:
            if contents != "":
                contents += ", "
            contents += "BOOT2A"
        if self.pkg_contents & FwuPkgInfoTLV.CONTENTS_MASK_BOOT2B:
            if contents != "":
                contents += ", "
            contents += "BOOT2B"
        if self.pkg_contents & FwuPkgInfoTLV.CONTENTS_MASK_APP:
            if contents != "":
                contents += ", "
            contents += "APP"
        if self.pkg_contents & FwuPkgInfoTLV.CONTENTS_MASK_BASEPATCH:
            if contents != "":
                contents += ", "
            contents += "BASEPATCH"
        if self.pkg_contents & FwuPkgInfoTLV.CONTENTS_MASK_LOWENERGY:
            if contents != "":
                contents += ", "
            contents += "BLEPATCH"
        print("  Pkg Contents: " + contents)
        print("  Pkg Raw Payload Offset: " + str(self.raw_payload_offset))


class MinimumVersionDependencyTLV(TLV):
    _tlv_payload_size = 3
    _tlv_type = TLV._TLV_TYPE_MINIMUM_VERSRION_DEPENDENCY

    # 4 and 5 are for BOOT2A and BOOT2B, respectively
    IMAGE_TYPE_BOOT1 = 1
    IMAGE_TYPE_BOOT2 = 2
    IMAGE_TYPE_APP = 3
    IMAGE_TYPE_PATCH = 6

    def __init__(self, image_type=-1, major_version=-1, minor_version=-1):
        TLV.__init__(self)
        self._image_type = image_type
        self._min_major_ver = major_version
        self._min_minor_ver = minor_version

    @property
    def image_type(self):
        return self._image_type

    @property
    def minimum_major_version(self):
        return self._min_major_ver

    @property
    def minimum_minor_version(self):
        return self._min_minor_ver

    def _payload_to_string(self):
        res = ""

        # Image Type (uint8)
        res += chr(self._image_type)

        # Minimum Major Version (uint8)
        res += chr(self._min_major_ver)

        # Minimum Minor Version (uint8)
        res += chr(self._min_minor_ver)

        return res

    def _payload_from_string(self, source=None, start_loc=0):
        source_loc = start_loc

        self._image_type = ord(source[source_loc])
        source_loc += 1

        self._min_major_ver = ord(source[source_loc])
        source_loc += 1

        self._min_minor_ver = ord(source[source_loc])

    def print_payload_summary(self, verbose=False):
        image_type = "unknown"
        if self.image_type == MinimumVersionDependencyTLV.IMAGE_TYPE_BOOT1:
            image_type = "BOOT1"
        if self.image_type == MinimumVersionDependencyTLV.IMAGE_TYPE_BOOT2:
            image_type = "BOOT2"
        if self.image_type == MinimumVersionDependencyTLV.IMAGE_TYPE_APP:
            image_type = "APP"
        if self.image_type == MinimumVersionDependencyTLV.IMAGE_TYPE_PATCH:
            image_type = "PATCH"
        print("  Dep Image Type: " + image_type)
        print("  Minimum Version: " + str(self.minimum_major_version) + "." + str(self.minimum_minor_version))


class BinaryImageTLV(TLV):
    _tlv_payload_size = 32
    _tlv_type = TLV._TLV_TYPE_BINARY_IMAGE

    BINARY_IMAGE_TYPE_BOOT1 = 1
    BINARY_IMAGE_TYPE_BOOT2_PIC = 2
    BINARY_IMAGE_TYPE_APP = 3
    BINARY_IMAGE_TYPE_BOOT2A = 4
    BINARY_IMAGE_TYPE_BOOT2B = 5

    def __init__(self, image_type=None, major_version=None, minor_version=None, version_tag=None,
                 raw_image_offset=0xFFFFFFFF, raw_image_size=None, raw_image_crc=None, raw_image_loc=None):
        TLV.__init__(self)
        self._image_type = image_type
        self._major_version = major_version
        self._minor_version = minor_version
        self._version_tag = version_tag
        self._raw_image_offset = raw_image_offset
        self._raw_image_loc = raw_image_loc

        if raw_image_loc is not None:
            fh = io.FileIO(raw_image_loc, "r")
            raw_image_contents = fh.readall()
            fh.close()
            self._raw_image_size = len(raw_image_contents)
            self._raw_image_crc = BinaryImageTLV._calc_crc(raw_image_loc)
        else:
            self._raw_image_size = raw_image_size
            self._raw_image_crc = raw_image_crc

    @property
    def image_type(self):
        return self._image_type

    @property
    def major_version(self):
        return self._major_version

    @property
    def minor_version(self):
        return self._minor_version

    @property
    def version_tag(self):
        return self._version_tag

    @classmethod
    def _calc_crc(self, image_loc):
        result = 0xFFFFFFFF
        fh = io.FileIO(image_loc, "r")
        raw_image = fh.readall()
        result = binascii.crc32(raw_image) & 0xFFFFFFFF
        fh.close()
        return result

    @property
    def raw_image_offset(self):
        return self._raw_image_offset

    @raw_image_offset.setter
    def raw_image_offset(self, value):
        self._raw_image_offset = value

    @property
    def raw_image_crc(self):
        return self._raw_image_crc

    @raw_image_crc.setter
    def raw_image_crc(self, value):
        self._raw_image_crc = value

    @property
    def raw_image_size(self):
        return self._raw_image_size

    @raw_image_size.setter
    def raw_image_size(self, value):
        self._raw_image_size = value

    @classmethod
    def parse_version_string(cls, value):
        split = value.split(".", 1)
        split = [split[0]] + split[1].split("-",1)
        split[0] = int(split[0])
        split[1] = int(split[1])
        if len(split) == 2:
            split.append("")
        return split

    def _payload_to_string(self):
        res = ""

        # Image Type (uint8)
        res += chr(self._image_type)

        # Major Version (uint8)
        res += chr(self._major_version)

        # Minor Version (uint8)
        res += chr(self._minor_version)

        # Version Tag (char[17])
        if self._version_tag:
            res += self._version_tag[:17].ljust(17, chr(0x0)) # make sure length <= 48 and if less then pad right with 0x00
        else:
            res += chr(0x00)*17

        # Image Size (uint32)
        res += int32_to_le_hexstr(self.raw_image_size)

        # Image CRC (uint32)
        res += int32_to_le_hexstr(self.raw_image_crc)

        # Image offset (uint32)
        res += int32_to_le_hexstr(self._raw_image_offset)

        return res

    def _payload_from_string(self, source=None, start_loc=0):
        source_loc = start_loc

        # Image Type (uint8)
        self._image_type = ord(source[source_loc])
        source_loc += 1

        # Major Version (uint8)
        self._major_version = ord(source[source_loc])
        source_loc += 1

        # Minor Version (uint8)
        self._minor_version = ord(source[source_loc])
        source_loc += 1

        # Version Tag (char[17])
        self._version_tag = source[source_loc:source_loc + 17].replace(chr(0), " ").rstrip()
        source_loc += 17

        # Image Size (uint32)
        self._raw_image_size = le_hexstr_to_int32(source, source_loc)
        source_loc += 4

        # Image CRC (uint32)
        self._raw_image_crc = le_hexstr_to_int32(source, source_loc)
        source_loc += 4

        # Image offset (uint32)
        self._raw_image_offset = le_hexstr_to_int32(source, source_loc)

    @classmethod
    def binary_image_type_to_str(cls, type):
        image_type = "Unknown"
        if type == BinaryImageTLV.BINARY_IMAGE_TYPE_BOOT1:
            image_type = "BOOT1"
        elif type == BinaryImageTLV.BINARY_IMAGE_TYPE_BOOT2_PIC:
            image_type = "BOOT2-PIC"
        elif type == BinaryImageTLV.BINARY_IMAGE_TYPE_APP:
            image_type = "APP"
        elif type == BinaryImageTLV.BINARY_IMAGE_TYPE_BOOT2A:
            image_type = "BOOT2A"
        elif type == BinaryImageTLV.BINARY_IMAGE_TYPE_BOOT2B:
            image_type = "BOOT2B"
        elif type == BinaryImageTLV.BINARY_IMAGE_TYPE_PATCH:
            image_type = "PATCH"

        return image_type

    def print_payload_summary(self, verbose=False):
        image_type = BinaryImageTLV.binary_image_type_to_str(self.image_type)
        print("  Image Type: " + image_type)
        version_string = str(self.major_version) + "." + str(self.minor_version)
        if self.version_tag is not None and self.version_tag != "":
            version_string += "-" + self.version_tag
        print("  Image Version: " +  version_string)
        print("  Image Size: " + str(self.raw_image_size))
        print("  Image CRC: 0x" + hex(self.raw_image_crc))
        print("  Image Offset (within package): 0x" + hex(self.raw_image_offset))

class PatchTLV(TLV):
    _tlv_payload_size = 32
    _tlv_type = TLV._TLV_TYPE_PATCH

    PATCH_TYPE_BASEPATCH = 1
    PATCH_TYPE_LOWENERGY = 2

    def __init__(self, patch_type=None,
                 raw_patch_offset=0xFFFFFFFF, raw_patch_size=None, raw_patch_crc=None, raw_patch_loc=None):
        TLV.__init__(self)
        self._patch_type = patch_type
        self._raw_patch_size = raw_patch_size
        self._raw_patch_offset = raw_patch_offset
        self._raw_patch_loc = raw_patch_loc

        if raw_patch_loc is not None:
            fh = io.open(raw_patch_loc, "rb")
            raw_patch_contents = fh.read()
            fh.close()
            self._raw_patch_size = len(raw_patch_contents)
            self._raw_patch_crc = PatchTLV._calc_crc(raw_patch_loc)
        else:
            self._raw_patch_size = raw_patch_size
            self._raw_patch_crc = raw_patch_crc
    
    @property
    def patch_type(self):
        return self._patch_type

    @classmethod
    def _calc_crc(self, patch_loc):
        result = 0xFFFFFFFF
        fh = io.FileIO(patch_loc, "rb")
        raw_patch = fh.readall()
        result = binascii.crc32(raw_patch) & 0xFFFFFFFF
        fh.close()
        return result

    @property
    def raw_patch_offset(self):
        return self._raw_patch_offset

    @raw_patch_offset.setter
    def raw_patch_offset(self, value):
        self._raw_patch_offset = value

    @property
    def raw_patch_crc(self):
        return self._raw_patch_crc

    @raw_patch_crc.setter
    def raw_patch_crc(self, value):
        self._raw_patch_crc = value

    @property
    def raw_patch_size(self):
        return self._raw_patch_size

    @raw_patch_size.setter
    def raw_patch_size(self, value):
        self._raw_patch_size = value

    def _payload_to_string(self):
        res = ""

        # Patch Type (uint8)
        res += chr(self._patch_type)

        # Patch Size (uint32)
        res += int32_to_le_hexstr(self.raw_patch_size)

        # Patch CRC (uint32)
        res += int32_to_le_hexstr(self.raw_patch_crc)

        # Patch offset (uint32)
        res += int32_to_le_hexstr(self._raw_patch_offset)

        return res

    def _payload_from_string(self, source=None, start_loc=0):
        source_loc = start_loc

        # Patch Type (uint8)
        self._patch_type = ord(source[source_loc])
        source_loc += 1

        # Patch Size (uint32)
        self._raw_patch_size = le_hexstr_to_int32(source, source_loc)
        source_loc += 4

        # Patch CRC (uint32)
        self._raw_patch_crc = le_hexstr_to_int32(source, source_loc)
        source_loc += 4

        # Patch offset (uint32)
        self._raw_patch_offset = le_hexstr_to_int32(source, source_loc)

    @classmethod
    def patch_type_to_str(cls, type):
        patch_type = "Unknown"
        if type == PatchTLV.PATCH_TYPE_BASEPATCH:
            patch_type = "BASEPATCH"
        elif type == PatchTLV.PATCH_TYPE_LOWENERGY:
            patch_type = "LOWENERGY"

        return patch_type

    def print_payload_summary(self, verbose=False):
        patch_type = PatchTLV.patch_type_to_str(self.patch_type)
        print("  Patch Type: " + patch_type)
        print("  Patch Size: " + str(self.raw_patch_size))
        print("  Patch CRC: 0x" + hex(self.raw_patch_crc))
        print("  Patch Offset (within package): 0x" + hex(self.raw_patch_offset))
