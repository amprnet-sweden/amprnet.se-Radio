# Build instructions

## Environment

Requirements:
- TI Code Composer Studio (CCS) version 12.8.1
- SIMPLELINK-LOWPOWER-F2-SDK 8.30.01.01
- CCS gcc plugin (View -> App Center -> See more -> Arm gcc)

Import the .cproject file into CCS.

### Building for each radio

The included .cproject file contains build configurations for each radio variant, RC (RadioControlli) and N536, and no additional setup should be required.

The build configuration is selected in the drop-down menu by the build icon.

To add support for building for each radio to an existing .cproject file, or add a new radio variant:
- Create a build configuration for each radio variant (Build configurations -> Manage ...)
- For each build configuration, exclude the other radio's syscfg from the build (right-click on the syscfg file and select Exclude from build)
- For the N536 build configuration, define the N536RADIO build variable.

Refer to:
https://software-dl.ti.com/ccs/esd/documents/application_notes/appnote-sysconfig_multiple_configuration_files.html
