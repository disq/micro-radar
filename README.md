<h1 align=center>
  📡 Micro Radar
</h1>
<h6 align=center>
  a tiny open-source flight radar for your desk
</h6>
<p align=center>
  <img src="https://github.com/user-attachments/assets/2ccb2063-d15c-4180-8e3c-ae3a81c814ff" alt="drawing" width="400"/>
</p>
<p align=center>
  <a href="#prerequisites">PREREQUISITES</a> - <a href="#assembly">ASSEMBLY</a> - <a href="#usage">USAGE</a>
</p>

## Prerequisites

At the core of this build is a **Raspberry Pi Pico W** paired with a **Pimoroni Pico Display Pack 2.0** — a 320x240 IPS screen that presses straight onto the Pico's headers. The Pico W handles WiFi and drives the screen over SPI.

> Looking for the original ESP32-C3 round-screen build? See the git history before this port.

### Tools you'll need

- A Raspberry Pi Pico W with headers already soldered (or a soldering iron to fit your own)
- A USB cable that supports data transfer

### Shopping List

- [ ] [Raspberry Pi Pico W (with pre-soldered headers)](https://shop.pimoroni.com/products/raspberry-pi-pico-w)
- [ ] [Pimoroni Pico Display Pack 2.0 (320x240 IPS)](https://shop.pimoroni.com/products/pico-display-pack-2-0)
- [ ] A USB cable to suit your Pico

### Accounts / API

This project uses OpenSky's API for retrieving flight data.

I highly recommend making an account, as it's free, and allows the radar to make many more requests per day (400 -> 4000), which makes the live view much more accurate. However, it isn't necessary if you prefer.

You can sign up [here](https://opensky-network.org), or search "OpenSky".

Further info on what to do with the account is in the usage section.

## Assembly

With the Pico W **unplugged**, line the Pico Display Pack 2.0 up with the Pico's pin headers (the screen sits over the USB end of the board) and press it down firmly until it's fully seated. That's the whole build — no soldering, screws, or glue required.

Then plug the Pico W into USB and flash the firmware (see [Usage](#usage)).

> **Note:** the 3D-printable enclosure under [`./hardware`](./hardware) was designed for the original round ESP32-C3 build and does **not** fit the Pico Display 2.0. It's kept for reference only.

## Usage

### Flashing the Firmware

You'll need [VS Code](https://code.visualstudio.com/) with the [PlatformIO IDE extension](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide) installed. Once installed, restart VS Code, open the repository folder, and dependencies will pull in automatically (the first build downloads the Pico toolchain, so give it a few minutes).

For the very first upload, hold the **BOOTSEL** button on the Pico W while plugging it into USB — it'll mount as a `RPI-RP2` drive. Release BOOTSEL, then hit the upload button (→) in the bottom status bar. After the first flash, PlatformIO can usually reset and re-upload over USB without touching BOOTSEL.

If you hit an upload failure, try:

- Disconnect and reconnect the USB cable (holding BOOTSEL again)
- Check that your cable supports data transfer (some are charge-only)
- Try a different USB port on your computer

Read more about PlatformIO [here](https://docs.platformio.org/en/latest/).

### First Boot

On first boot, the radar broadcasts a WiFi hotspot called `MicroRadar-Setup`. Connect to it from your phone or laptop and a configuration page will appear automatically (or go to your browser if it doesn't). Enter your WiFi credentials and hit save. The board will restart and connect to your network.

If the hotspot doesn't appear straight away, give it a moment. If it still hasn't appeared after 30 seconds, exit the WiFi settings on your device and go back in to force a refresh. It'll usually show up then.

### Configuration

Once connected to your network, the radar config is accessible at [http://microradar.local](http://microradar.local) from any device on the same network.

Here you can set:

- **Location** (latitude and longitude): the centre point of your radar
- **Radar radius**: how wide the scan extends (in degrees, 2 degrees is the limit to avoid rate limiting)
- **Display options**: toggle visual elements
- **OpenSky credentials**: your client ID and secret (if you've made an account - again, highly recommend!)

<img width="400" alt="image" src="https://github.com/user-attachments/assets/45e6219c-2672-4197-baad-16ae08180b58" />

If you've made an OpenSky account (which I highly recommend), you can find your credentials under your account settings at opensky-network.org. With authentication, you get 4000 requests per day instead of 400, making the live view much more accurate. Read more about the API [here](https://opensky-network.org).

This configuration page is accessible anytime the device is connected to WiFi, so you can tweak settings whenever you want.

That's it! Once you've configured everything, you should see a live view of all flights over your location. Enjoy :)

<img width="400" alt="IMG_7935" src="https://github.com/user-attachments/assets/118b9a1c-c2c0-488d-b638-d8684a30b1d7" />

## Notes

> Designed and developed as part of a wedding present for a mate who loves aviation (congratulations to both him and his wife!)

> Inspired by [therealhacksaw](https://www.instagram.com/therealhacksaw/)'s desk radar

> Built with ♥︎ in London
