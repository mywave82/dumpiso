# Supported formats
## Small (debug) utility that inspects and dumps information from images that contains filesystems based uppon
* ECMA 119 / ISO 9660
  * System Use Sharing Protocol
    * RockRidge extentions
    * Amiga extensions
  * Joliet extensions
  * El Torrito boot extension
* ECMA 167 / ISO/IEC 13346 / UDF 2.60 (only 2048 bytes sector support)

## Images can have various low-level sector formats:
* 2048 bytes per sector (only standard data is provided)
* 2056 bytes per sector (XA1 - includes subheader)
* 2352 bytes per sector (includes the entire user-data per sector, cdrdao --read-raw)
* 2448 bytes per sector (includes the entire user-data per sector and r-w subchannels, cdrdao --read-raw --read-subchan rw\_raw)

## Not included
* ECMA 168 / ISO/IEC 13490
* UDF 2.60 with 512/4096 bytes sector support


# Licensing
Until further notice, this code is released as public domain.

Documentation and references inside the "specs" directory are a collection fram various sources found online, each one entitled to their own copyrights.

