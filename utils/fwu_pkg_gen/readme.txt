1.  Need to create a .s19 file by building executable in Code Warrior.

2.  Convert that .s19 file into a bin file, using the burn.py script in the "burner" directory.
    burner.py -h

    Jim:  boot2a's orgin is 0x2000, boot2b's is 0x12000 and use length of 0xA000 [for bootloaders].

    Examples:

    *** BOOT2A  ***
    python burn.py -s BT_CERT_RELEASE_01\boot2A.afx.S19 -b BT_CERT_RELEASE_01\boot2A.bin -l 0xa000 -o 0x2000

    *** BOOT2B  ***
    python burn.py -s BT_CERT_RELEASE_01\boot2B.afx.S19 -b BT_CERT_RELEASE_01\boot2B.bin -l 0xa000 -o 0x12000

    *** MQX APP ***
    python burn.py -s BT_CERT_RELEASE_01\bt_cert_app.S19 -b BT_CERT_RELEASE_01\bt_cert_app.bin -l 0x33000 -o 0x22000


3.  Once you have the .bin app file, run this python script at MQX 3.8\utils\
    C:\Users\ernie\corona_fw\utils\fwu_pkg_gen>python fwu_pkg_gen.py -a burner\BT_CERT_RELEASE_01\boot2A.bin -b burn
er\BT_CERT_RELEASE_01\boot2B.bin -m burner\BT_CERT_RELEASE_01\bt_cert_app.bin -t "BT Cert for David Release 01" --app-ver="1.1" -o burn
er\BT_CERT_RELEASE_01\bt_cert_01.pkg --boot2a-ver="1.1" --boot2b-ver="1.1"

4.  Get info on an existing package file.
    fwu_pkg_gen.py -i
 pcba_02.pkg