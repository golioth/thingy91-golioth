..
   Copyright (c) 2024 Golioth, Inc.
   SPDX-License-Identifier: Apache-2.0

Golioth Thingy91 Example Program
#################################

Overview
********

This repository highlights various aspects of the Golioth platform on the Nordic
Thingy91 device.

This repo is based on the Golioth `Reference Design Template`_.

Local set up
************

.. pull-quote::
   [!IMPORTANT]

   Do not clone this repo using git. Zephyr's ``west`` meta tool should be used to
   set up your local workspace.

Install the Python virtual environment (recommended)
====================================================

.. code-block:: shell

   cd ~
   mkdir thingy91-golioth
   python -m venv thingy91-golioth/.venv
   source thingy91-golioth/.venv/bin/activate
   pip install wheel west

Use ``west`` to initialize and install
======================================

.. code-block:: shell

   cd ~/thingy91-golioth
   west init -m git@github.com:golioth/thingy91-golioth.git .
   west update
   west zephyr-export
   pip install -r deps/zephyr/scripts/requirements.txt

Building the application
************************

Build Zephyr sample application for the Thingy91 (``thingy91_nrf9160_ns``) from
the top level of your project. After a successful build you will see a new
``build`` directory. Note that any changes (and git commits) to the project
itself will be inside the ``app`` folder. The ``build`` and ``deps`` directories
being one level higher prevents the repo from cataloging all of the changes to
the dependencies and the build (so no ``.gitignore`` is needed).

Prior to building, update ``VERSION`` file to reflect the firmware version number you want to assign
to this build. Then run the following commands to build and program the firmware onto the device.


.. pull-quote::
   [!IMPORTANT]

   You must perform a pristine build (use ``-p`` or remove the ``build`` directory)
   after changing the firmware version number in the ``VERSION`` file for the change to take effect.

.. code-block:: text

   $ (.venv) west build -p -b thingy91/nrf9160/ns --sysbuild app
   $ (.venv) west flash

Configure PSK-ID and PSK using the device shell based on your Golioth
credentials and reboot:

.. code-block:: text

   uart:~$ settings set golioth/psk-id <my-psk-id@my-project>
   uart:~$ settings set golioth/psk <my-psk>
   uart:~$ kernel reboot cold

Add Pipeline to Golioth
***********************

Golioth uses `Pipelines`_ to route stream data. This gives you flexibility to change your data
routing without requiring updated device firmware.

Whenever sending stream data, you must enable a pipeline in your Golioth project to configure how
that data is handled. Add the contents of ``pipelines/cbor-to-lightdb.yml`` as a new pipeline as
follows (note that this is the default pipeline for new projects and may already be present):

   1. Navigate to your project on the Golioth web console.
   2. Select ``Pipelines`` from the left sidebar and click the ``Create`` button.
   3. Give your new pipeline a name and paste the pipeline configuration into the editor.
   4. Click the toggle in the bottom right to enable the pipeline and then click ``Create``.

All data streamed to Golioth in CBOR format will now be routed to LightDB Stream and may be viewed
using the web console. You may change this behavior at any time without updating firmware simply by
editing this pipeline entry.

Golioth Features
****************

This app currently implements Over-the-Air (OTA) firmware updates, Settings
Service, Logging, RPC, and both LightDB State and LightDB Stream data.

Settings Service
================

The following settings should be set in the Device Settings menu of the
`Golioth Console`_.

``LOOP_DELAY_S``
   Adjusts the delay between sensor readings. Set to an integer value (seconds).

   Default value is ``60`` seconds.

``LED_FADE_SPEED_MS``
   Adjusts the total LED fade time from 0.5 to 10 seconds. Set to an integer
   value (milliseconds).

   Default value is ``1200`` milliseconds.

``RED_INTENSITY_PCT``
   Adjusts brightness of onboard red LED. Set to an integer value (percentage).

   Default value is ``50`` percent.

``GREEN_INTENSITY_PCT``
   Adjusts brightness of onboard green LED. Set to an integer value
   (percentage).

   Default value is ``50`` percent.

``BLUE_INTENSITY_PCT``
   Adjusts brightness of onboard blue LED. Set to an integer value (percentage).

   Default value is ``50`` percent.

Remote Procedure Call (RPC) Service
===================================

The following RPCs can be initiated in the Remote Procedure Call menu of the
`Golioth Console`_.

``get_network_info``
   Query and return network information.

``reboot``
   Reboot the system.

``set_log_level``
   Set the log level.

   The method takes a single parameter which can be one of the following integer
   values:

   * ``0``: ``LOG_LEVEL_NONE``
   * ``1``: ``LOG_LEVEL_ERR``
   * ``2``: ``LOG_LEVEL_WRN``
   * ``3``: ``LOG_LEVEL_INF``
   * ``4``: ``LOG_LEVEL_DBG``

``play_song``
   This device can play different songs when the ``play_song`` RPC is sent with
   one of the following parameters:

   * ``beep``: Play a short 1 kHz tone. Also plays when button is pressed.
   * ``funkytown``: Play the main tune from the 70s classic.
   * ``mario``: Itsa me...a classic chiptune song!
   * ``golioth``: A short theme for Golioth. Also plays on device boot.

.. _Reference Design Template: https://github.com/golioth/reference-design-template
.. _Golioth Console: https://console.golioth.io
.. _golioth-zephyr-boards: https://github.com/golioth/golioth-zephyr-boards
