# New layouts in mango-ext

## Canvas

Mango-exts `canvas` layout is similar to window managers like hevel. It lets you move around floating windows on an infinite canvas (hence the name), move around your view along that canvas, pan between windows and zoom in and out.

### canvas-specific config options

|option|type|Description|
|:---|:---|:---|
|`canvas_tiling`|int| determines the position of newly spawned windows. see section `canvas_tiling options`|
|`canvas_tiling_gap`|int| determines the size of window gaps when using canvas_tiling|
|`canvas_pan_on_kill`|bool| wether or not to pan to another window when the current window is killed|

#### canvas_tiling options

|option|Description|
|:---|:---|
|0| spawns new windows in the center of the viewport|
|1| spawns new windows to the left of the focused window|
|2| spawns new windows to the right of the focused window|
|3| spawns new windows below the focused window|
|4| spawns new windows above the focused window|
|5| picks between 1 - 4 depending on the position of the mouse in relation to the focused window |

### canvas-specific dispatchers

|dispatcher|Description|
|:--- |:---|
|`canvas-overview-toggle`| toggles the canvas layouts' overview feature. This lets you see all of your windows at once and quickly reposition your view.|
|`toggleminimap`| toggles the minimap, which displays all of your windows along with a visual indicator of your current view.|
|`canvas_zoom_resize,<arg>`| zooms in/out, direction and strength are determined by <arg>|
|`canvas_fill_viewport`| resizes the currently focused window to make it fill up the entire current viewport|
|`canvas_centerview`| moves the viewport to center the currently focused window|


## Dwindle

The `dwindle` layout as known from WMs like bspwm and hyprland.Creating new windows splits the currently focused window either horizontally or vertically depending on its proportions.

