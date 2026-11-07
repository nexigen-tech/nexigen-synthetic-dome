import os
import sys
import subprocess
import json
import re

def run_analysis(project_path):
    metrics = {
        "files": {"cpp": 0, "headers": 0, "build_systems": 0},
        "lines": {"code": 0, "comments": 0, "blank": 0},
        "complexity": {"total_functions": 0, "high_complexity_functions": [], "avg_complexity": 0},
        "tech_debt": {"todo_count": 0, "fixme_count": 0}
    }

    # 1. Сбор базовых метрик строк кода через cloc (если доступен)
    try:
        res = subprocess.run(["cloc", project_path, "--json"], capture_output=True, text=True)
        if res.returncode == 0:
            cloc_data = json.loads(res.stdout)
            for lang in ["C++", "C/C++ Header"]:
                if lang in cloc_data:
                    metrics["lines"]["code"] += cloc_data[lang].get("code", 0)
                    metrics["lines"]["comments"] += cloc_data[lang].get("comment", 0)
                    metrics["lines"]["blank"] += cloc_data[lang].get("blank", 0)
                    if lang == "C++":
                        metrics["files"]["cpp"] = cloc_data[lang].get("nFiles", 0)
                    else:
                        metrics["files"]["headers"] = cloc_data[lang].get("nFiles", 0)
    except Exception:
        pass # Если cloc не установлен, агент соберет данные встроенными средствами

    # 2. Сканирование файлов сборки, сложности и техдолга вручную/через lizard
    total_complexity = 0
    
    for root, dirs, files in os.walk(project_path):
        # Игнорируем папки сборки и зависимостей
        if any(p in root for p in ["build", ".git", "vcpkg_installed", "node_modules"]):
            continue
            
        for file in files:
            # Считаем файлы сборки
            if file in ["CMakeLists.txt", "Makefile", "conanfile.txt"]:
                metrics["files"]["build_systems"] += 1
                
            # Ищем TODO и FIXME в C++ файлах
            if file.endswith(('.cpp', '.h', '.hpp', '.cc', '.cxx')):
                file_path = os.path.join(root, file)
                try:
                    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                        content = f.read()
                        metrics["tech_debt"]["todo_count"] += len(re.findall(r'\bTODO\b', content, re.IGNORECASE))
                        metrics["tech_debt"]["fixme_count"] += len(re.findall(r'\bFIXME\b', content, re.IGNORECASE))
                except Exception:
                    pass

    # Использование lizard для подсчета цикломатической сложности функций
    try:
        import lizard
        analysis = lizard.analyze([project_path])
        # Новые версии lizard возвращают генератор FileInformation
        if hasattr(analysis, 'function_list'):
            func_lists = [analysis.function_list]
        else:
            func_lists = [file_info.function_list for file_info in analysis]
        for func_list in func_lists:
            for func in func_list:
                metrics["complexity"]["total_functions"] += 1
                total_complexity += func.cyclomatic_complexity
                if func.cyclomatic_complexity > 15: # Порог высокой сложности
                    metrics["complexity"]["high_complexity_functions"].append({
                        "name": func.name,
                        "complexity": func.cyclomatic_complexity,
                        "filename": func.filename
                    })
        
        funcs_cnt = metrics["complexity"]["total_functions"]
        metrics["complexity"]["avg_complexity"] = round(total_complexity / funcs_cnt, 2) if funcs_cnt > 0 else 0
    except ImportError:
        pass

    print(json.dumps(metrics, indent=2, ensure_ascii=False))

if __name__ == "__main__":
    path = sys.argv[1] if len(sys.argv) > 1 else "."
    run_analysis(path)
