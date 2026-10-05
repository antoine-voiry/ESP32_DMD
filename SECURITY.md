# Security

## Reporting a vulnerability

Please open a [private security advisory](https://github.com/antoine-voiry/ESP32_DMD/security/advisories/new)
rather than a public issue.

## What the firmware trusts

The board is meant for a home or club network, next to Raspydarts. It protects itself against
other web sites and against malformed input, but not against someone already on that network.

| Input | Protection |
|---|---|
| Web pages | Requests must use the board's own name or IP address (blocks DNS rebinding). Form posts must come from the board's own pages (blocks cross-site requests). Posts are rate limited. All values shown in pages are HTML-escaped. |
| Uploads | Only into the media folders, with sanitised file names. `config.json` cannot be replaced or deleted. Truncated uploads are removed. |
| MQTT | Payloads are validated before use. At most 32 waiting messages are kept (older ones are dropped). |
| Setup portal | Protected by a random password, shown on the panel. It never opens without a password. |
| Media files | PNGs larger than 1024 px are refused; text images are capped in width. |
| Weather / Tempo | Responses are limited to 48 KB, parsed defensively, and only displayed. |

## Known limitations

- **No login on the web pages.** Anyone on the network can change settings, upload files or
  restart the board, as with the original Raspy2DMD.
- **MQTT has no authentication.** Like Raspydarts, the firmware connects anonymously; anyone who can
  publish to the broker can drive the panel (including `rebt` and `shutdwn`).
- **HTTPS certificates are not verified** for OpenWeatherMap and the Tempo API, so someone able to
  intercept the board's traffic could read the OpenWeatherMap API key or alter the forecast.
- `receipconf` publishes every setting, including the OpenWeatherMap API key, to the Raspydarts
  topic, as the Pi version did.
