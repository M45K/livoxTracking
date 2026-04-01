#include "debug_view.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>

namespace {

std::string EscapeHtml(const std::string& input) {
  std::string output;
  output.reserve(input.size());
  for (const char c : input) {
    switch (c) {
      case '&': output += "&amp;"; break;
      case '<': output += "&lt;"; break;
      case '>': output += "&gt;"; break;
      case '"': output += "&quot;"; break;
      case '\'': output += "&#39;"; break;
      default: output += c; break;
    }
  }
  return output;
}

double MapX(const DebugViewConfig& cfg, float x) {
  const double normalized = (x - cfg.x_min) / static_cast<double>(cfg.x_max - cfg.x_min);
  return normalized * cfg.width;
}

double MapY(const DebugViewConfig& cfg, float y) {
  const double normalized = (y - cfg.y_min) / static_cast<double>(cfg.y_max - cfg.y_min);
  return cfg.height - normalized * cfg.height;
}

std::string TrackColor(std::uint64_t id) {
  const int hue = static_cast<int>((id * 57ULL) % 360ULL);
  std::ostringstream stream;
  stream << "hsl(" << hue << " 75% 55%)";
  return stream.str();
}

void WriteAtomically(const std::string& path, const std::string& content) {
  const std::filesystem::path output_path(path);
  if (output_path.has_parent_path()) {
    std::filesystem::create_directories(output_path.parent_path());
  }

  const std::filesystem::path temp_path = output_path.string() + ".tmp";
  {
    std::ofstream file(temp_path, std::ios::binary | std::ios::trunc);
    file << content;
  }
  std::error_code ec;
  std::filesystem::remove(output_path, ec);
  std::filesystem::rename(temp_path, output_path);
}

std::string BuildGridSvg(const DebugViewConfig& cfg) {
  std::ostringstream svg;
  svg << "<g stroke=\"#273043\" stroke-width=\"1\">";
  const int x_start = static_cast<int>(std::floor(cfg.x_min));
  const int x_end = static_cast<int>(std::ceil(cfg.x_max));
  for (int x = x_start; x <= x_end; ++x) {
    const double sx = MapX(cfg, static_cast<float>(x));
    svg << "<line x1=\"" << sx << "\" y1=\"0\" x2=\"" << sx << "\" y2=\"" << cfg.height << "\" />";
  }
  const int y_start = static_cast<int>(std::floor(cfg.y_min));
  const int y_end = static_cast<int>(std::ceil(cfg.y_max));
  for (int y = y_start; y <= y_end; ++y) {
    const double sy = MapY(cfg, static_cast<float>(y));
    svg << "<line x1=\"0\" y1=\"" << sy << "\" x2=\"" << cfg.width << "\" y2=\"" << sy << "\" />";
  }
  svg << "</g>";
  return svg.str();
}

}  // namespace

DebugViewRenderer::DebugViewRenderer(const AppConfig& config) : config_(config) {}

bool DebugViewRenderer::Enabled() const {
  return config_.debug_view.enabled;
}

void DebugViewRenderer::Render(std::uint64_t timestamp_ms,
                               const std::vector<PointXYZI>& points,
                               const std::vector<Detection>& detections,
                               const std::vector<PersonState>& people,
                               const std::vector<ZoneSnapshot>& zones,
                               const std::vector<LidarStats>& lidar_stats) {
  if (!Enabled()) {
    return;
  }
  if (last_render_ms_ != 0 && timestamp_ms - last_render_ms_ < 500) {
    return;
  }
  last_render_ms_ = timestamp_ms;

  const DebugViewConfig& view = config_.debug_view;
  std::ostringstream html;
  html << std::fixed << std::setprecision(2);
  html << "<!doctype html><html><head><meta charset=\"utf-8\">"
       << "<meta http-equiv=\"refresh\" content=\"1\">"
       << "<title>Livox Debug View</title>"
       << "<style>"
       << ":root{color-scheme:dark;--bg:#0b1020;--panel:#121a2e;--accent:#2dd4bf;--grid:#273043;--text:#e5eef8;--muted:#8aa0bf;}"
       << "body{margin:0;font-family:ui-sans-serif,system-ui,sans-serif;background:radial-gradient(circle at top,#17203a,#0b1020 60%);color:var(--text);}"
       << ".wrap{display:grid;grid-template-columns:minmax(780px,1fr) 360px;gap:16px;padding:16px;min-height:100vh;box-sizing:border-box;}"
       << ".panel{background:rgba(18,26,46,.92);border:1px solid rgba(138,160,191,.18);border-radius:18px;box-shadow:0 18px 40px rgba(0,0,0,.25);overflow:hidden;}"
       << ".head{padding:14px 16px;border-bottom:1px solid rgba(138,160,191,.12);display:flex;justify-content:space-between;align-items:center;}"
       << ".title{font-size:18px;font-weight:700;letter-spacing:.03em;}"
       << ".badge{color:#0b1020;background:var(--accent);padding:4px 10px;border-radius:999px;font-weight:700;font-size:12px;}"
       << ".body{padding:16px;}"
       << ".stats{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:14px;}"
       << ".card{background:rgba(255,255,255,.03);border:1px solid rgba(138,160,191,.12);border-radius:14px;padding:12px;}"
       << ".label{font-size:11px;text-transform:uppercase;letter-spacing:.08em;color:var(--muted);margin-bottom:6px;}"
       << ".value{font-size:24px;font-weight:700;}"
       << ".small{font-size:13px;color:var(--muted);}"
       << "table{width:100%;border-collapse:collapse;font-size:13px;}"
       << "th,td{text-align:left;padding:8px 0;border-bottom:1px solid rgba(138,160,191,.1);}"
       << "th{color:var(--muted);font-weight:600;font-size:11px;text-transform:uppercase;letter-spacing:.08em;}"
       << "svg{display:block;width:100%;height:auto;background:linear-gradient(180deg,#0f162c,#0a1020);}"
       << ".legend{display:flex;gap:12px;flex-wrap:wrap;padding:12px 16px;color:var(--muted);font-size:12px;border-top:1px solid rgba(138,160,191,.12);}"
       << ".dot{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:6px;}"
       << "</style></head><body><div class=\"wrap\">";

  html << "<section class=\"panel\"><div class=\"head\"><div class=\"title\">Room Top View</div>"
       << "<div class=\"badge\">auto refresh</div></div>";
  html << "<svg viewBox=\"0 0 " << view.width << ' ' << view.height << "\" xmlns=\"http://www.w3.org/2000/svg\">";
  html << BuildGridSvg(view);

  for (const auto& zone : config_.zones) {
    const double x = MapX(view, zone.x_min);
    const double y = MapY(view, zone.y_max);
    const double w = MapX(view, zone.x_max) - x;
    const double h = MapY(view, zone.y_min) - y;
    html << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w << "\" height=\"" << h
         << "\" fill=\"rgba(45,212,191,.08)\" stroke=\"rgba(45,212,191,.65)\" stroke-dasharray=\"8 6\"/>";
    html << "<text x=\"" << (x + 8.0) << "\" y=\"" << (y + 18.0)
         << "\" font-size=\"14\" fill=\"#9be7dd\">" << EscapeHtml(zone.name) << "</text>";
  }

  const std::size_t step = std::max<std::size_t>(1, points.size() / std::max<std::size_t>(1, view.max_points));
  html << "<g fill=\"#8fb3ff\" fill-opacity=\"0.55\">";
  for (std::size_t i = 0; i < points.size(); i += step) {
    const auto& point = points[i];
    const double x = MapX(view, point.position.x);
    const double y = MapY(view, point.position.y);
    if (x < 0.0 || x > view.width || y < 0.0 || y > view.height) {
      continue;
    }
    html << "<circle cx=\"" << x << "\" cy=\"" << y << "\" r=\"1.6\"/>";
  }
  html << "</g>";

  for (const auto& lidar : config_.lidars) {
    const double x = MapX(view, lidar.T.data[3]);
    const double y = MapY(view, lidar.T.data[7]);
    html << "<g>";
    html << "<circle cx=\"" << x << "\" cy=\"" << y << "\" r=\"8\" fill=\"#f59e0b\" stroke=\"#fff4d6\" stroke-width=\"2\"/>";
    html << "<text x=\"" << (x + 12.0) << "\" y=\"" << (y - 10.0)
         << "\" font-size=\"13\" fill=\"#fcd34d\">" << EscapeHtml(lidar.name) << "</text>";
    html << "</g>";
  }

  for (const auto& detection : detections) {
    const double x = MapX(view, detection.min_bounds.x);
    const double y = MapY(view, detection.max_bounds.y);
    const double w = MapX(view, detection.max_bounds.x) - x;
    const double h = MapY(view, detection.min_bounds.y) - y;
    html << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << std::max(4.0, w)
         << "\" height=\"" << std::max(4.0, h)
         << "\" fill=\"rgba(56,189,248,.10)\" stroke=\"#38bdf8\" stroke-width=\"2\"/>";
  }

  for (const auto& person : people) {
    const std::string color = TrackColor(person.id);
    const double x = MapX(view, person.position.x);
    const double y = MapY(view, person.position.y);
    const double vx = MapX(view, person.position.x + person.velocity.x * 0.4f);
    const double vy = MapY(view, person.position.y + person.velocity.y * 0.4f);
    html << "<g>";
    html << "<line x1=\"" << x << "\" y1=\"" << y << "\" x2=\"" << vx << "\" y2=\"" << vy
         << "\" stroke=\"" << color << "\" stroke-width=\"3\" stroke-linecap=\"round\"/>";
    html << "<circle cx=\"" << x << "\" cy=\"" << y << "\" r=\"7.5\" fill=\"" << color
         << "\" stroke=\"#ffffff\" stroke-width=\"2\"/>";
    html << "<text x=\"" << (x + 10.0) << "\" y=\"" << (y - 10.0)
         << "\" font-size=\"14\" font-weight=\"700\" fill=\"#ffffff\">#" << person.id << "</text>";
    html << "</g>";
  }

  html << "</svg>";
  html << "<div class=\"legend\">"
       << "<span><span class=\"dot\" style=\"background:#8fb3ff\"></span>dynamic points</span>"
       << "<span><span class=\"dot\" style=\"background:#38bdf8\"></span>detections</span>"
       << "<span><span class=\"dot\" style=\"background:#f59e0b\"></span>lidars</span>"
       << "<span><span class=\"dot\" style=\"background:#2dd4bf\"></span>zones</span>"
       << "</div></section>";

  html << "<aside class=\"panel\"><div class=\"head\"><div class=\"title\">System Status</div>"
       << "<div class=\"small\">ts " << timestamp_ms << "</div></div><div class=\"body\">";
  html << "<div class=\"stats\">";
  html << "<div class=\"card\"><div class=\"label\">Active Tracks</div><div class=\"value\">" << people.size() << "</div></div>";
  html << "<div class=\"card\"><div class=\"label\">Detections</div><div class=\"value\">" << detections.size() << "</div></div>";
  html << "<div class=\"card\"><div class=\"label\">Dynamic Points</div><div class=\"value\">" << points.size() << "</div></div>";
  html << "<div class=\"card\"><div class=\"label\">Zones</div><div class=\"value\">" << zones.size() << "</div></div>";
  html << "</div>";

  html << "<div class=\"card\" style=\"margin-bottom:14px;\"><div class=\"label\">Zones</div><table><thead><tr><th>Name</th><th>Occupancy</th><th>IDs</th></tr></thead><tbody>";
  if (zones.empty()) {
    html << "<tr><td colspan=\"3\" class=\"small\">No zones configured</td></tr>";
  } else {
    for (const auto& zone : zones) {
      html << "<tr><td>" << EscapeHtml(zone.name) << "</td><td>" << zone.Occupancy() << "</td><td>";
      for (std::size_t i = 0; i < zone.occupant_ids.size(); ++i) {
        if (i != 0) {
          html << ", ";
        }
        html << zone.occupant_ids[i];
      }
      html << "</td></tr>";
    }
  }
  html << "</tbody></table></div>";

  html << "<div class=\"card\" style=\"margin-bottom:14px;\"><div class=\"label\">Tracks</div><table><thead><tr><th>ID</th><th>X</th><th>Y</th><th>Vx</th><th>Vy</th></tr></thead><tbody>";
  if (people.empty()) {
    html << "<tr><td colspan=\"5\" class=\"small\">No active tracks</td></tr>";
  } else {
    for (const auto& person : people) {
      html << "<tr><td>#" << person.id << "</td><td>" << person.position.x << "</td><td>"
           << person.position.y << "</td><td>" << person.velocity.x << "</td><td>" << person.velocity.y << "</td></tr>";
    }
  }
  html << "</tbody></table></div>";

  html << "<div class=\"card\"><div class=\"label\">Lidars</div><table><thead><tr><th>Name</th><th>Connected</th><th>Packets</th><th>Points</th></tr></thead><tbody>";
  for (const auto& lidar : lidar_stats) {
    html << "<tr><td>" << EscapeHtml(lidar.name) << "</td><td>"
         << (lidar.connected ? "yes" : "no") << "</td><td>" << lidar.packets
         << "</td><td>" << lidar.points << "</td></tr>";
  }
  html << "</tbody></table></div>";

  html << "</div></aside></div></body></html>";

  WriteAtomically(config_.debug_view.file, html.str());
}
