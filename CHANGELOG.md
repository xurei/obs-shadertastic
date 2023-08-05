# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Added
- Added Parameter type `image`
- Added Parameter type `source`
- Added "Reset time when shown" parameter on type-dependant filters
- Filters : Pixelate effect

### Changed
- Using RGBA with 16bit channels for intermediate textures
- Parameters are now ordered as they appear in the meta file

### Fixed
- Fixed time increased multiple times when a source is duplicated

## [0.0.2] - 2023-06-24
### Added 
- Added the metadata field `input_time` defaulting to `false`. This is a breaking change for time-dependent filters
- Added boolean parameters in effects

### Changed
- Slightly improved performance of the filters rendering
- Gaussian blur and Pixelate transition : changed `breaking_point` 0->100 to `break_point` 0->1
- Gaussian blur filter : default levels to 10 (to avoid lags on small configs)
- Filters : reset rand_seed on reload

### Fixed
- Fixed multi-step rendering of filter and transitions when the source is not full-screen
- Rewritten `shadertastic_filter_get_color_space` to match a filter, not a transition
- Fixed transparent texture not created correctly
- Filter reload should update the filter UI without the need to switch filters

### Removed
- Removed dead code used for debugging purposes
