/* gamerzilla.vapi
 *
 * Copyright (C) 2020 Dennis Payne
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 *
 * Author:
 * 	Dennis Payne <dulsi@identicalsoftware.com>
 */

[CCode (cheader_filename = "gamerzilla.h")]
namespace Gamerzilla {
	[CCode (cname="GamerzillaStart")]
	public static int start(bool server, string savedir);

	[CCode (cname="GamerzillaConnect")]
	public static int connect(string url, string username, string password);

	[CCode (cname="GamerzillaGetGame")]
	public static int getGame(string short_name);

	[CCode (cname="GamerzillaFreeGame")]
	public static int freeGame(int game_id);

	[CCode (cname="GamerzillaGetGameImage")]
	public static int getGameImage(int game_id);

	[CCode (cname="GamerzillaServerProcess")]
	public static int process(Posix.timeval? timeout);

	[CCode (cname="GamerzillaQuit")]
	public static void quit();
}
