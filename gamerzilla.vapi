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
	[CCode (cname = "GamerzillaAccessGame", has_target = true)]
	public delegate void AccessFunc(string short_name, string name);

	[CCode (cname="GamerzillaStart")]
	public static int start(bool server, string savedir);

	[CCode (cname="GamerzillaConnect")]
	public static int connect(string url, string username, string password);

	[CCode (cname="GamerzillaGetGame")]
	public static int get_game(string short_name);

	[CCode (cname="GamerzillaFreeGame")]
	public static int free_game(int game_id);

	[CCode (cname="GamerzillaGetGameImage")]
	public static string get_game_image(int game_id);

	[CCode (cname="GamerzillaGetTrophyNum")]
	public static int get_trophy_num(int game_id);

	[CCode (cname="GamerzillaGetTrophyByIndex")]
	public static void get_trophy_by_index(int game_id, int indx, out unowned string name, out unowned string desc);

	[CCode (cname="GamerzillaGetTrophyImage")]
	public static string get_trophy_image(int game_id, string name, bool achieved);

	[CCode (cname="GamerzillaGetTrophy")]
	public static bool get_trophy(int game_id, string name, out bool achieved);

	[CCode (cname="GamerzillaServerProcess")]
	public static int process(Posix.timeval? timeout);

	[CCode (cname="GamerzillaServerListen")]
	public static void listen(AccessFunc callback);

	[CCode (cname="GamerzillaQuit")]
	public static void quit();
}
