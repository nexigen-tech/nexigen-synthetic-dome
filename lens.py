import os
import sys
import json
import yaml
import click
from git import Repo
from datetime import datetime

class ProjectAnalyzer:
    def __init__(self, repo_path: str):
        if not os.path.exists(os.path.join(repo_path, ".git")):
            raise ValueError(f"Директория '{repo_path}' не является Git-репозиторием.")
        self.repo_path = repo_path
        self.repo = Repo(repo_path)
        
        self.lang_mapping = {
            '.cpp': 'C++', '.hpp': 'C++', '.cc': 'C++', '.h': 'C++',
            '.rs': 'Rust', '.py': 'Python', '.cs': 'C#'
        }
        
        self.ignored_dirs = {
            'node_modules', 'venv', '.venv', 'env', 'target', 
            'bin', 'obj', '.git', 'build', 'cmake-build-debug', 'dist'
        }

    def should_ignore(self, filepath: str) -> bool:
        parts = set(filepath.split(os.sep))
        return not parts.isdisjoint(self.ignored_dirs)

    def is_test_file(self, filepath: str) -> bool:
        lower_path = filepath.lower()
        return any(x in lower_path for x in ["test", "spec", "steps"]) and not lower_path.endswith(('.yaml', '.yml'))

    def analyze_openspec(self, file_content: str):
        try:
            data = yaml.safe_load(file_content)
            if not isinstance(data, dict): return 0, 0
            paths = data.get("paths", {})
            endpoints_count = len(paths)
            
            described_endpoints = 0
            for path, methods in paths.items():
                if isinstance(methods, dict):
                    for method, details in methods.items():
                        if isinstance(details, dict) and "description" in details:
                            described_endpoints += 1
                            break
            return endpoints_count, described_endpoints
        except:
            return 0, 0

    def collect_data(self, branch: str, limit: int):
        dynamics = []
        try:
            commits = list(self.repo.iter_commits(branch, max_count=limit))
            commits.reverse()
        except Exception as e:
            click.secho(f"Ошибка чтения ветки {branch}: {str(e)}", fg="red")
            sys.exit(1)

        with click.progressbar(commits, label="Анализ истории репозитория") as bar:
            for commit in bar:
                commit_date = datetime.fromtimestamp(commit.committed_date).strftime('%d.%m')
                
                metrics = {
                    "languages": {lang: 0 for lang in set(self.lang_mapping.values())},
                    "test_lines": 0, "markdown_lines": 0,
                    "openspec_endpoints": 0, "openspec_documented": 0
                }

                for entry in commit.tree.traverse():
                    if entry.type == 'blob':
                        if self.should_ignore(entry.path): continue
                        try:
                            content = entry.data_stream.read().decode('utf-8', errors='ignore')
                            lines_count = len(content.splitlines())
                            _, ext = os.path.splitext(entry.path.lower())

                            if entry.path.endswith('.md'):
                                metrics["markdown_lines"] += lines_count
                            elif entry.path.endswith(('.yaml', '.yml', '.json')) and 'spec' in entry.path.lower():
                                endpoints, documented = self.analyze_openspec(content)
                                metrics["openspec_endpoints"] += endpoints
                                metrics["openspec_documented"] += documented
                            elif self.is_test_file(entry.path):
                                metrics["test_lines"] += lines_count
                            elif ext in self.lang_mapping:
                                metrics["languages"][self.lang_mapping[ext]] += lines_count
                        except:
                            continue

                dynamics.append({"commit": commit.hexsha[:7], "date": commit_date, "metrics": metrics})
        return dynamics
def generate_svg_line_chart(data, value_keys, colors, width=500, height=250):
    padding = 40
    chart_w = width - padding * 2
    chart_h = height - padding * 2
    
    max_val = 0
    for item in data:
        for k in value_keys:
            val = item["metrics"]["languages"].get(k, 0) if k in ['C++', 'Rust', 'Python', 'C#'] else item["metrics"].get(k, 0)
            max_val = max(max_val, val)
    
    if max_val == 0: max_val = 100
    pow10 = 10 ** max(0, len(str(int(max_val))) - 1)
    max_val = ((int(max_val) // pow10) + 1) * pow10
    
    num_points = len(data)
    x_step = chart_w / max(1, num_points - 1)
    
    svg = f'<svg width="100%" height="100%" viewBox="0 0 {width} {height}" xmlns="http://w3.org">'
    svg += '<style>text { font-family: sans-serif; font-size: 10px; fill: #64748b; } .grid { stroke: #e2e8f0; stroke-width: 1; }</style>'
    
    for i in range(5):
        y_val = max_val * (i / 4)
        y_pos = height - padding - (y_val / max_val * chart_h)
        svg += f'<line x1="{padding}" y1="{y_pos}" x2="{width-padding}" y2="{y_pos}" class="grid" />'
        svg += f'<text x="{padding-10}" y="{y_pos+4}" text-anchor="end">{int(y_val)}</text>'
        
    for i, item in enumerate(data):
        x_pos = padding + i * x_step
        if num_points <= 15 or i % (num_points // 10 + 1) == 0:
            svg += f'<line x1="{x_pos}" y1="{padding}" x2="{x_pos}" y2="{height-padding}" class="grid" stroke-dasharray="2,2" />'
            svg += f'<text x="{x_pos}" y="{height-padding+15}" text-anchor="middle" transform="rotate(30, {x_pos}, {height-padding+15})">{item["date"]}</text>'

    for idx, k in enumerate(value_keys):
        color = colors[idx % len(colors)]
        points = []
        for i, item in enumerate(data):
            x_pos = padding + i * x_step
            val = item["metrics"]["languages"].get(k, 0) if k in ['C++', 'Rust', 'Python', 'C#'] else item["metrics"].get(k, 0)
            y_pos = height - padding - (val / max_val * chart_h)
            points.append(f"{x_pos},{y_pos}")
            
        svg += f'<polyline points="{" ".join(points)}" fill="none" stroke="{color}" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"/>'
        for pt in points:
            x, y = pt.split(',')
            svg += f'<circle cx="{x}" cy="{y}" r="3" fill="white" stroke="{color}" stroke-width="2"/>'

    svg += '</svg>'
    return svg

HTML_TEMPLATE = """<!DOCTYPE html>
<html>
<head>
    <title>OpenCode Lens Report</title>
    <style>
        body {{ font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #f4f6f9; margin: 0; padding: 40px; color: #333; }}
        .container {{ max-width: 1200px; margin: 0 auto; }}
        h1 {{ margin-bottom: 5px; color: #1e293b; }}
        .subtitle {{ color: #64748b; margin-bottom: 30px; }}
        .grid {{ display: grid; grid-template-columns: 1fr 1fr; gap: 30px; margin-bottom: 30px; }}
        .card {{ background: white; padding: 24px; border-radius: 12px; box-shadow: 0 4px 6px -1px rgba(0,0,0,0.05); }}
        .card.full {{ grid-column: span 2; }}
        h3 {{ margin-top: 0; margin-bottom: 10px; color: #334155; }}
        .legend {{ display: flex; gap: 15px; margin-bottom: 15px; font-size: 12px; }}
        .legend-item {{ display: flex; align-items: center; gap: 5px; }}
        .color-box {{ width: 12px; height: 12px; border-radius: 3px; }}
    </style>
</head>
<body>
    <div class="container">
        <h1>📊 OpenCode Lens</h1>
        <div class="subtitle">Динамика развития проекта: <strong>{repo_name}</strong> (Ветка: {branch})</div>
        <div class="grid">
            <div class="card full">
                <h3>Динамика кодовой базы (Строки кода по языкам)</h3>
                <div class="legend">
                    <div class="legend-item"><div class="color-box" style="background:#f34b7d"></div>C++</div>
                    <div class="legend-item"><div class="color-box" style="background:#dea584"></div>Rust</div>
                    <div class="legend-item"><div class="color-box" style="background:#3572A5"></div>Python</div>
                    <div class="legend-item"><div class="color-box" style="background:#178600"></div>C#</div>
                </div>
                {chart_code}
            </div>
            <div class="card">
                <h3>Спецификации OpenSpec / API</h3>
                <div class="legend">
                    <div class="legend-item"><div class="color-box" style="background:#6366f1"></div>Всего эндпоинтов</div>
                    <div class="legend-item"><div class="color-box" style="background:#10b981"></div>С описанием</div>
                </div>
                {chart_spec}
            </div>
            <div class="card">
                <h3>Тесты и Документация</h3>
                <div class="legend">
                    <div class="legend-item"><div class="color-box" style="background:#f59e0b"></div>Строки тестов</div>
                    <div class="legend-item"><div class="color-box" style="background:#ec4899"></div>Строки Markdown (.md)</div>
                </div>
                {chart_docs}
            </div>
        </div>
    </div>
</body>
</html>
"""

@click.command()
@click.option('--path', '-p', default='.', help='Путь к репозиторию Git')
@click.option('--branch', '-b', default=None, help='Ветка для анализа')
@click.option('--limit', '-l', default=30, help='Максимальное количество коммитов')
@click.option('--output', '-o', default='opencode_report.html', help='Имя выходного файла')
def main(path, branch, limit, output):
    """CLI утилита для визуализации динамики разработки проектов на OpenCode / OpenSpec."""
    click.secho("🚀 Инициализация OpenCode Lens (Автономный SVG)...", fg="cyan", bold=True)
    try:
        analyzer = ProjectAnalyzer(path)
    except ValueError as e:
        click.secho(str(e), fg="red")
        sys.exit(1)
        
    if not branch: branch = analyzer.repo.active_branch.name
    data = analyzer.collect_data(branch, limit)
    repo_name = os.path.basename(os.path.abspath(path))

    svg_code = generate_svg_line_chart(data, ['C++', 'Rust', 'Python', 'C#'], ['#f34b7d', '#dea584', '#3572A5', '#178600'], width=1100, height=300)
    svg_spec = generate_svg_line_chart(data, ['openspec_endpoints', 'openspec_documented'], ['#6366f1', '#10b981'])
    svg_docs = generate_svg_line_chart(data, ['test_lines', 'markdown_lines'], ['#f59e0b', '#ec4899'])

    html_content = HTML_TEMPLATE.format(
        repo_name=repo_name, branch=branch,
        chart_code=svg_code, chart_spec=svg_spec, chart_docs=svg_docs
    )

    with open(output, 'w', encoding='utf-8') as f:
        f.write(html_content)

    click.secho(f"\n✨ Автономный отчет готов!", fg="green", bold=True)
    click.echo(f"👉 Откройте в браузере файл: {os.path.abspath(output)}")

if __name__ == '__main__':
    main()
