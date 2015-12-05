# Example of generating a release build and package.
builder.py --app 0.1-e130711 --d_usb --odir c:\temp --genpkg

# Bootloader Only
builder.py --boot2 0.16-130903 --d_uart --odir C:\temp --genpkg

# Build without dependencies:
SAME AS ABOVE but with --nodep
