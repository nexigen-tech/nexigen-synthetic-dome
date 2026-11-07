import os
import sys
import json
import subprocess

def run_command(cmd, cwd):
    try:
        result = subprocess.run(cmd, shell=True, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        return {
            "exit_code": result.returncode,
            "stdout": result.stdout,
            "stderr": result.stderr
        }
    except Exception as e:
        return {"exit_code": -1, "stdout": "", "stderr": str(e)}

def count_unsafe_and_complexity(project_dir):
    """Сканирует исходный код для базовой оценки сложности и рисков (unsafe-блоки)"""
    unsafe_count = 0
    total_lines = 0
    
    for root, _, files in os.walk(project_dir):
        if "target" in root.split(os.sep) or ".git" in root.split(os.sep):
            continue
        for file in files:
            if file.endswith(".rs"):
                path = os.path.join(root, file)
                try:
                    with open(path, "r", encoding="utf-8", errors="ignore") as f:
                        content = f.read()
                        total_lines += len(content.splitlines())
                        unsafe_count += content.count("unsafe ") + content.count("unsafe{")
                except:
                    pass
    return {"total_lines": total_lines, "unsafe_blocks_count": unsafe_count}

def main():
    if len(sys.argv) < 2:
        project_dir = os.getcwd()
    else:
        project_dir = os.path.abspath(sys.argv[1])

    if not os.path.exists(os.path.join(project_dir, "Cargo.toml")):
        print(json.dumps({"error": f"Cargo.toml не найден в директории {project_dir}"}, ensure_ascii=False))
        sys.exit(1)

    report = {
        "project_path": project_dir,
        "metrics": {},
        "issues": [],
        "suggested_fixes": [],
        "auto_fixes_applied": []
    }

    # 0. Сбор базовых метрик сложности кода
    code_metrics = count_unsafe_and_complexity(project_dir)
    report["metrics"].update(code_metrics)

    # 1. АВТО-ИСПРАВЛЕНИЕ: Запуск cargo clippy --fix «на лету»
    # Это исправит большинство стилистических предупреждений до основного анализа
    fix_res = run_command("cargo clippy --fix --allow-no-vcs", project_dir)
    if fix_res["exit_code"] == 0:
        report["auto_fixes_applied"].append({
            "tool": "clippy-fix",
            "message": "Базовые предупреждения стиля и идиоматичности успешно исправлены автоматически."
        })

    # 2. Проверка форматирования (rustfmt)
    fmt_res = run_command("cargo fmt --check", project_dir)
    report["metrics"]["formatting_ok"] = (fmt_res["exit_code"] == 0)
    if fmt_res["exit_code"] != 0:
        report["issues"].append({
            "tool": "rustfmt",
            "level": "warning",
            "message": "Код не отформатирован по стандарту."
        })
        report["suggested_fixes"].append({
            "description": "Автоматически отформатировать весь код проекта по стандарту Rust",
            "command": "cargo fmt"
        })

    # 3. Глубокий статический анализ (Clippy) после авто-исправлений
    # Собираем только те ошибки, которые clippy не смог исправить сам
    clippy_cmd = "cargo clippy --all-targets --message-format=json"
    clippy_res = run_command(clippy_cmd, project_dir)
    
    clippy_errors = 0
    clippy_warnings = 0
    
    for line in clippy_res["stdout"].splitlines():
        try:
            data = json.loads(line)
            if data.get("reason") == "compiler-message":
                msg = data.get("message", {})
                level = msg.get("level")
                rendered = msg.get("rendered", "")
                
                if level in ["warning", "error"]:
                    if level == "error": clippy_errors += 1
                    else: clippy_warnings += 1
                    
                    report["issues"].append({
                        "tool": "clippy",
                        "level": level,
                        "message": rendered.strip()
                    })
        except:
            continue

    report["metrics"]["clippy_errors"] = clippy_errors
    report["metrics"]["clippy_warnings"] = clippy_warnings

    # Если после авто-исправлений всё ещё остались ошибки компиляции
    if clippy_errors > 0:
        report["suggested_fixes"].append({
            "description": "Попробовать исправить оставшиеся ошибки компиляции с помощью cargo fix",
            "command": "cargo fix --allow-no-vcs"
        })

    # 4. Аудит безопасности (cargo audit)
    audit_res = run_command("cargo audit --json", project_dir)
    if audit_res["exit_code"] in [127, -1]:
        report["metrics"]["security_audit"] = "Not installed"
    else:
        try:
            audit_data = json.loads(audit_res["stdout"])
            vulnerabilities = audit_data.get("vulnerabilities", {}).get("list", [])
            report["metrics"]["vulnerabilities_count"] = len(vulnerabilities)
            
            for vuln in vulnerabilities:
                advisory = vuln.get("advisory", {})
                report["issues"].append({
                    "tool": "cargo-audit",
                    "level": "critical",
                    "message": f"Уязвимость в {advisory.get('package')}: {advisory.get('title')} ({advisory.get('id')})"
                })
            
            if len(vulnerabilities) > 0:
                report["suggested_fixes"].append({
                    "description": "Обновить зависимости в Cargo.lock до безопасных версий",
                    "command": "cargo update"
                })
        except:
            report["metrics"]["security_audit"] = "Parse error or no vulns"

    # Выводим финальный JSON для OpenCode
    print(json.dumps(report, ensure_ascii=False, indent=2))

if __name__ == "__main__":
    main()
