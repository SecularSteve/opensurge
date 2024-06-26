// -----------------------------------------------------------------------------
// File: show_message.ss
// Description: a function object that displays a Message Box
// Author: Alexandre Martins <http://opensurge2d.org>
// License: MIT
// -----------------------------------------------------------------------------
using SurgeEngine.Level;
using SurgeEngine.Vector2;

//
// Show Message is a function object that displays a Message Box
//
// Arguments:
// - text: string. The message to be displayed.
//
object "Show Message"
{
    fun call(message)
    {
        mb = Level.spawnEntity("Message Box", Vector2.zero);
        mb.text = message;
    }
}