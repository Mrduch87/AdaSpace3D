#!/bin/bash
echo "Installing udev rules for AdaSpace3D..."

# Create rule to allow all users to read/write to the SpaceMouse
# Match by the 3DConnexion VID/PID we are spoofing (256f:c631)
echo 'KERNEL=="ttyACM*", ATTRS{idVendor}=="256f", ATTRS{idProduct}=="c631", MODE="0666"' | sudo tee /etc/udev/rules.d/99-spacemouse.rules > /dev/null

echo "Reloading rules..."
sudo udevadm control --reload-rules
sudo udevadm trigger

echo "Done! You can now use the Configurator without sudo."
