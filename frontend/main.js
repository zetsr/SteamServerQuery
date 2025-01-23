const terminal = document.getElementById('terminal');
const commandInput = document.getElementById('command-input');
const submitButton = document.getElementById('submit-button');

// 初始化终端
terminal.innerHTML = 'Welcome to the Dark Command Line Interface\n';
terminal.innerHTML += 'Enter server IP and port (e.g., 127.0.0.1:27015) in the input box below.\n';

// 提交函数
function submitCommand() {
    const input = commandInput.value.trim();
    if (input) {
        const [ip, port] = input.split(':');
        if (!ip || !port) {
            terminal.innerHTML += 'Invalid input format. Please use IP:PORT (e.g., 127.0.0.1:27015)\n';
            return;
        }

        // 显示用户输入
        terminal.innerHTML += `> ${input}\n`;

        // 发送请求到后端
        fetch('/query', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ ip, port }),
        })
        .then(response => response.text())
        .then(data => {
            terminal.innerHTML += `${data}\n`;
        })
        .catch(error => {
            terminal.innerHTML += `Error: ${error}\n`;
        });

        // 清空输入框
        commandInput.value = '';

        // 滚动到底部
        terminal.scrollTop = terminal.scrollHeight;
    }
}

// 绑定按钮点击事件
submitButton.addEventListener('click', submitCommand);

// 绑定输入框回车事件
commandInput.addEventListener('keypress', (event) => {
    if (event.key === 'Enter') {
        submitCommand();
    }
});