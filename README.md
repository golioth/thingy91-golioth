# Golioth Thingy91 Example Program

This repository highlights various aspects of the Golioth platform on
the Nordic Thingy91 and Thingy91x devices.

Using a pre-compiled binary (or one you build yourself), you can
provision the device via USB and connect to the Golioth cloud.
Time-series readings for temperature, pressure, humidity, accelerometer,
and various other sensors will periodically be sent to the cloud. You
can manage the device from the Golioth web console, configuring
settings, running RPCs, viewing logs, and performing Over-the-Air
firmware update (OTA).

This repository contains the firmware source code and [pre-built release
firmware
images](https://github.com/golioth/thingy91-golioth/releases).

## Supported Hardware
- Nordic Thingy:91
- Nordic Thingy:91X

### Additional Sensors/Components

The Thingy91 Example Program doesn't require any additional sensors or
components.

## Golioth Features

This app implements:

  - [Device Settings
    Service](https://docs.golioth.io/firmware/golioth-firmware-sdk/device-settings-service)
  - [Remote Procedure Call
    (RPC)](https://docs.golioth.io/firmware/golioth-firmware-sdk/remote-procedure-call)
  - [Stream
    Client](https://docs.golioth.io/firmware/golioth-firmware-sdk/stream-client)
  - [LightDB State
    Client](https://docs.golioth.io/firmware/golioth-firmware-sdk/light-db-state/)
  - [Over-the-Air (OTA) Firmware
    Upgrade](https://docs.golioth.io/firmware/golioth-firmware-sdk/firmware-upgrade/firmware-upgrade)
  - [Backend
    Logging](https://docs.golioth.io/device-management/logging/)

### Settings Service

The following settings should be set in the Device Settings menu of the
[Golioth Console](https://console.golioth.io).

  - `LOOP_DELAY_S`
    Adjusts the delay between sensor readings. Set to an integer value
    (seconds).

  - `LED_FADE_SPEED_MS`
    Adjusts the total LED fade time from 0.5 to 10 seconds. Set to an
    integer value (milliseconds).

    Default value is `1200` milliseconds.

  - `RED_INTENSITY_PCT`
    Adjusts brightness of onboard red LED. Set to an integer value
    (percentage).

    Default value is `50` percent.

  - `GREEN_INTENSITY_PCT`
    Adjusts brightness of onboard green LED. Set to an integer value
    (percentage).

    Default value is `50` percent.

  - `BLUE_INTENSITY_PCT`
    Adjusts brightness of onboard blue LED. Set to an integer value
    (percentage).

    Default value is `50` percent.

### Remote Procedure Call (RPC) Service

The following RPCs can be initiated in the Remote Procedure Call menu of
the [Golioth Console](https://console.golioth.io).

  - `get_network_info`
    Query and return network information.

  - `reboot`
    Reboot the system.

  - `set_log_level`
    Set the log level.

    The method takes a single parameter which can be one of the
    following integer values:

      - `0`: `LOG_LEVEL_NONE`
      - `1`: `LOG_LEVEL_ERR`
      - `2`: `LOG_LEVEL_WRN`
      - `3`: `LOG_LEVEL_INF`
      - `4`: `LOG_LEVEL_DBG`

  - `play_song`
    The Thingy91 can play different songs when the `play_song` RPC is
    sent with one of the following parameters:

      - `beep`: Play a short 1 kHz tone. Also plays when button is
        pressed.
      - `funkytown`: Play the main tune from the 70s classic.
      - `mario`: Itsa me...a classic chiptune song\!
      - `golioth`: A short theme for Golioth. Also plays on device boot.

    Note that the Thingy91x does not have a buzzer and will return an
    "unimplemented" error code which this method is called.

### Time-Series Stream data

Sensor data is sent to Golioth based on the `LOOP_DELAY_S` setting.
Sensor vary between the supported boards, so different readings are
available based on your hardware. Data may be viewed in the [Golioth
Console](https://console.golioth.io) by viewing the LightDB Stream tab
of the device, or the in the Project's Monitor section on the left
sidebar.

Below you will find sample data for the devices supported by this
application.

#### Thingy91

``` json
{
   "sensor": {
      "accel": {
         "x": 0.343232,
         "y": -0.156906,
         "z": -9.257477
      },
      "light": {
         "blue": 23,
         "green": 56,
         "ir": 6,
         "red": 29
      },
      "weather": {
         "gas": 51344,
         "hum": 35.593,
         "pre": 98.548,
         "tem": 22.62
      }
   }
}
```

#### Thingy91x

``` json
{
   "sensor": {
      "accel": {
         "x": -0.008085,
         "y": 0.0294,
         "z": -0.803845
      },
      "weather": {
         "co2": 467.279876,
         "hum": 30.058282,
         "iaq": 35,
         "pre": 98511,
         "tem": 20.995311,
         "voc": 0.43105
      }
   }
}
```

### Stateful Data (LightDB State)

Up-counting and down-counting timer readings are periodically sent to
the `actual` path of the LightDB Stream service. The frequency that
these readings change is based on the `LOOP_DELAY_S` setting.

  - `desired` values may be changed from the cloud side. The device will
    recognize these, validate them for \[0..9999\] bounding, and then
    reset these endpoints to `-1`. Changes may be made while the device
    is not connected and will persist until the next time a connection
    is established.
  - `actual` values will be updated by the device whenever a valid value
    is received from the `desired` paths. The cloud may read the
    `state` endpoints to determine device status, but only the device
    should ever write to the `state` paths.

``` json
{
    "desired": {
        "counter_down": -1,
        "counter_up": -1
    },
    "state": {
        "counter_down": 9268,
        "counter_up": 731
    }
}
```

### OTA Firmware Update

This application includes the ability to perform Over-the-Air (OTA)
firmware updates. To do so, you need a binary compiled with a different
version number than what is currently running on the device.

> [!NOTE]
> If a newer release is available than what your device is currently
> running, you may download the pre-compiled binary that ends in
> `_update.bin` and use it in step 2 below.

1.  Update the version number in the `VERSION` file and perform a
    pristine (important) build to incorporate the version change.
2.  Upload the `build/app/zephyr/zephyr.signed.bin` file as a Package
    for your Golioth project.
      - Use either `thingy91` or `thingy91x` as the package name,
        depending on which board the update file was built for. (These
        package names were configured in this repository's board `.conf`
        files.)
      - Use the same version number from step 1.
3.  Create a Cohort for your device type (thingy91 or thingy91x)
4.  Create a Deployment for your Cohort using the package name and
    version number from step 2.
5.  Devices in your Cohort will automatically upgrade to the most
    recently deployed firmware.

Visit [the Golioth Docs OTA Firmware Upgrade
page](https://docs.golioth.io/firmware/golioth-firmware-sdk/firmware-upgrade/firmware-upgrade)
for more info.

This repo is based on the Golioth [Reference Design
Template](https://github.com/golioth/reference-design-template).

## Add Pipeline to Golioth

Golioth uses [Pipelines](https://docs.golioth.io/data-routing) to route
stream data. This gives you flexibility to change your data routing
without requiring updated device firmware.

Whenever sending stream data, you must enable a pipeline in your Golioth
project to configure how that data is handled. Add the contents of
`pipelines/cbor-to-lightdb-with-path.yml` as a new pipeline as follows
(note that this is the default pipeline for new projects and may already
be present):

1.  Navigate to your project on the Golioth web console.
2.  Select `Pipelines` from the left sidebar and click the `Create`
    button.
3.  Give your new pipeline a name and paste the pipeline configuration
    into the editor.
4.  Click the toggle in the bottom right to enable the pipeline and
    then click `Create`.

All data streamed to Golioth in CBOR format will now be routed to
LightDB Stream and may be viewed using the web console. You may change
this behavior at any time without updating firmware simply by editing
this pipeline entry.

## Local set up

> [!IMPORTANT]
> Do not clone this repo using git. Zephyr's `west` meta tool should be
> used to set up your local workspace.

### Install the Python virtual environment (recommended)

``` shell
cd ~
mkdir thingy91-golioth
python -m venv thingy91-golioth/.venv
source thingy91-golioth/.venv/bin/activate
pip install wheel west ecdsa
```

### Use `west` to initialize and install

``` shell
cd ~/thingy91-golioth
west init -m git@github.com:golioth/thingy91-golioth.git .
west update
west zephyr-export
pip install -r deps/zephyr/scripts/requirements.txt
```

## Building the application

Build Zephyr sample application for the Thingy91 (`thingy91_nrf9160_ns`)
from the top level of your project. After a successful build you will
see a new `build` directory. Note that any changes (and git commits) to
the project itself will be inside the `app` folder. The `build` and
`deps` directories being one level higher prevents the repo from
cataloging all of the changes to the dependencies and the build (so no
`.gitignore` is needed).

Prior to building, update `VERSION` file to reflect the firmware version
number you want to assign to this build. Then run the following commands
to build and program the firmware onto the device.

> [!WARNING]
> You must perform a pristine build (use `-p` or remove the `build`
> directory) after changing the firmware version number in the `VERSION`
> file for the change to take effect.

### Build for Thingy91

``` text
west build -p -b thingy91/nrf9160/ns --sysbuild app
west flash
```

### Build for Thingy91x

``` text
west build -p -b thingy91x/nrf9151/ns --sysbuild app
west flash --erase
```

## Provision the device

Configure PSK-ID and PSK using the device shell based on your Golioth
credentials and reboot:

``` text
uart:~$ settings set golioth/psk-id <my-psk-id@my-project>
uart:~$ settings set golioth/psk <my-psk>
uart:~$ kernel reboot cold
```

## External Libraries

The following code libraries are installed by default. If you are not
using the custom hardware to which they apply, you can safely remove
these repositories from `west.yml` and remove the includes/function
calls from the C code.

  - [zephyr-network-info](https://github.com/golioth/zephyr-network-info)
    is a helper library for querying, formatting, and returning network
    connection information via Zephyr log or Golioth RPC

## Have Questions?

Please get in touch with Golioth engineers by starting a new thread on
the [Golioth Forum](https://forum.golioth.io/).
