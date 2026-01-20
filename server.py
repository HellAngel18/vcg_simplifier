import os
import subprocess
import uuid
from flask import Flask, request, send_file, render_template

app = Flask(__name__)

# ================= 配置区域 =================
# 【重要】请修改为你编译出的 C++ 工具的实际路径
CLI_PATH = "./build/vcg-simplifier" 

# 临时文件存储目录
TEMP_DIR = "temp_uploads"
if not os.path.exists(TEMP_DIR):
    os.makedirs(TEMP_DIR)
# ===========================================

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/simplify', methods=['POST'])
def simplify():
    try:
        if 'file' not in request.files:
            return "No file uploaded", 400
        
        file = request.files['file']
        if file.filename == '':
            return "No file selected", 400

        # 1. 获取参数
        ratio = request.form.get('ratio', '0.5')
        target_count = request.form.get('target_count', '0')
        quality = request.form.get('quality', '0.3')
        texture_weight = request.form.get('texture_weight', '1.0')
        boundary_weight = request.form.get('boundary_weight', '1.0')
        # Checkbox 没勾选时 formData 里没有这个 key，勾选了值为 'on' 或 'true'
        preserve_boundary = '1' if request.form.get('preserve_boundary') == 'true' else '0'
        preserve_topology = '1' if request.form.get('preserve_topology') == 'true' else '0'

        # 2. 保存输入文件
        # 为了安全，使用 UUID 生成唯一文件名
        ext = os.path.splitext(file.filename)[1]
        unique_name = str(uuid.uuid4())
        input_filename = f"{unique_name}_input{ext}"
        # 强制输出为 GLB，因为 model-viewer 对 GLB 支持最好
        output_filename = f"{unique_name}_output.glb"
        
        input_path = os.path.join(TEMP_DIR, input_filename)
        output_path = os.path.join(TEMP_DIR, output_filename)
        
        file.save(input_path)

        # 3. 构建命令行指令
        # 对应你的 C++ main.cpp 参数: -i, -o, -r, -tc, -q, -tw, -bw, -pb, -pt
        cmd = [
            CLI_PATH,
            "-i", input_path,
            "-o", output_path,
            "-r", str(ratio),
            "-tc", str(target_count),
            "-q", str(quality),
            "-tw", str(texture_weight),
            "-bw", str(boundary_weight),
            "-pb", preserve_boundary,
            "-pt", preserve_topology
        ]

        print(f"Executing: {' '.join(cmd)}") # 打印日志方便调试

        # 4. 调用 C++ 程序
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode != 0:
            print(f"Error Output: {result.stderr}")
            return f"Simplification failed: {result.stderr}", 500

        # 5. 返回结果文件
        return send_file(output_path, as_attachment=True, download_name="simplified.glb")

    except Exception as e:
        print(e)
        return str(e), 500
    finally:
        # (可选) 清理输入文件，输出文件由 send_file 处理后通常保留一小段时间，这里暂不自动删除以免文件锁问题
        # 生产环境建议使用后台任务定期清理 temp 目录
        if os.path.exists(input_path):
            os.remove(input_path)

if __name__ == '__main__':
    app.run(debug=True, port=5000)