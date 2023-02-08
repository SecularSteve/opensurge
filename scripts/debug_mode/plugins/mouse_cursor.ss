// -----------------------------------------------------------------------------
// File: mouse_cursor.ss
// Description: mouse cursor plugin (Debug Mode)
// Author: Alexandre Martins <http://opensurge2d.org>
// License: MIT
// -----------------------------------------------------------------------------

/*

This plugin displays a mouse cursor on Desktop computers.

*/

using SurgeEngine.Input.MobileGamepad;
using SurgeEngine.Input.Mouse;
using SurgeEngine.Transform;
using SurgeEngine.Actor;

object "Debug Mode - Mouse Cursor" is "debug-mode-plugin", "detached", "private", "entity"
{
    actor = Actor("Mouse Cursor");
    transform = Transform();
    debugMode = parent;

    state "main"
    {
        // this entity is in screen space, not in world space
        transform.position = Mouse.position;
    }

    fun init()
    {
        uiSettings = debugMode.plugin("Debug Mode - UI Settings");

        actor.zindex = uiSettings.zindex;
        actor.visible = !MobileGamepad.available; // display only if not on mobile
    }
}