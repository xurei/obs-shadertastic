# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [0.0.2] - 2023-06-24
### Added 
- Added the metadata field `input_time` defaulting to `false`. This is a breaking change for time-dependent filters
- Added boolean parameters in effects

### Changed
- Slightly improved performance of the filters rendering
- Gaussian blur transition : changed `breaking_point` 0->100 to `break_point` 0->1
- Filters : reset rand_seed on reload

### Fixed
- Fixed multi-step rendering of filter and transitions when the source is not full-screen
- Rewritten `shadertastic_filter_get_color_space` to match a filter, not a transition
- Fixed transparent texture not created correctly
- Filter reload should update the filter UI without the need to switch filters

### Removed
- Removed dead code used for debugging purposes
