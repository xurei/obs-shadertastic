# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Fixed
- Fixed crash and/or inability to switch the Scene Collection when there is a circular reference of sources

## [0.0.5] - 2023-10-26
### Added
- Added Parameter type `color`
- Added support for `.shadertastic` bundled effects
- Ink drop transition
- Displacement transition
- Displacement Map filter

### Changed
- Dev mode setting, disabling the "Reload" and "Auto Reload" options by default

### Fixed
- Fixed transition image parameter not being reloaded in auto reload mode
- Fixed param type `source` messing up the colors

## [0.0.4] - 2023-09-29
### Fixed
- Fixed built-in transition effects not loading when multiple transitions are created

## [0.0.3] - 2023-08-30
### Added
- Added Parameter type `image`
- Added Parameter type `source`
- Added Parameter type `audiolevel`
- Added "Additionnal effects" folder config, found in Tools > Shadertastic Settings 
- Added "Reset time on toggle" parameter on time-dependant filters
- Filters : "Pixelate" effect
- Filters : "Shake on sound" effect

### Changed
- Using RGBA with 16bit channels for intermediate textures
- Parameters are now ordered as they appear in the meta file
- Performance improvement : shaders now load only once, instead of creating new shaders for each filter

### Fixed
- Fixed crash when no effect is chosen in a transition (issue #2)
- Fixed multistep transitions handling of color in RGBA mode
- Fixed time increased multiple times when a source is duplicated
- Fixed incorrect copyright name in comments 

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
