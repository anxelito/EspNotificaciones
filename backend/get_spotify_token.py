"""One-off helper: obtain a Spotify refresh token for the Now Playing feature.

Usage (on your PC, not the server):
    python get_spotify_token.py <CLIENT_ID> <CLIENT_SECRET>

Prerequisite: in your Spotify developer app, add this exact Redirect URI:
    http://127.0.0.1:8888/callback

A browser opens, you log in / accept, and the script prints the three lines
to paste into the server's backend/.env. Standard library only.
"""
import base64
import http.server
import json
import secrets
import sys
import urllib.parse
import urllib.request
import webbrowser

REDIRECT_URI = "http://127.0.0.1:8888/callback"
SCOPES = "user-read-currently-playing user-read-playback-state"


def main() -> None:
    if len(sys.argv) != 3:
        sys.exit("usage: python get_spotify_token.py <CLIENT_ID> <CLIENT_SECRET>")
    client_id, client_secret = sys.argv[1], sys.argv[2]
    state = secrets.token_urlsafe(16)
    result: dict = {}

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):  # noqa: N802
            q = urllib.parse.parse_qs(urllib.parse.urlparse(self.path).query)
            ok = q.get("state", [""])[0] == state and "code" in q
            if ok:
                result["code"] = q["code"][0]
            else:
                result["error"] = q.get("error", ["bad state/response"])[0]
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            msg = "Listo, vuelve a la terminal." if ok else "Error: " + result["error"]
            self.wfile.write(f"<h2>{msg}</h2>".encode())

        def log_message(self, *args):
            pass

    auth_url = "https://accounts.spotify.com/authorize?" + urllib.parse.urlencode({
        "client_id": client_id,
        "response_type": "code",
        "redirect_uri": REDIRECT_URI,
        "scope": SCOPES,
        "state": state,
    })
    print("Abriendo navegador. Si no se abre, pega esta URL:\n" + auth_url)
    webbrowser.open(auth_url)

    with http.server.HTTPServer(("127.0.0.1", 8888), Handler) as srv:
        while not result:
            srv.handle_request()

    if "code" not in result:
        sys.exit("Spotify devolvio error: " + result["error"])

    basic = base64.b64encode(f"{client_id}:{client_secret}".encode()).decode()
    req = urllib.request.Request(
        "https://accounts.spotify.com/api/token",
        data=urllib.parse.urlencode({
            "grant_type": "authorization_code",
            "code": result["code"],
            "redirect_uri": REDIRECT_URI,
        }).encode(),
        headers={"Authorization": "Basic " + basic,
                 "Content-Type": "application/x-www-form-urlencoded"},
    )
    with urllib.request.urlopen(req, timeout=15) as resp:
        tokens = json.load(resp)

    print("\nPega esto en backend/.env del servidor:\n")
    print(f"SPOTIFY_CLIENT_ID={client_id}")
    print(f"SPOTIFY_CLIENT_SECRET={client_secret}")
    print(f"SPOTIFY_REFRESH_TOKEN={tokens['refresh_token']}")


if __name__ == "__main__":
    main()
