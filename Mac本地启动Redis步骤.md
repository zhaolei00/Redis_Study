1. 从github上 git clone Redis源码。
2. cd到项目中，执行make命令。
3. Visual Studio 需要安装名为"CodeLLDB"的扩展。
4. 在项目中创建.vscode文件夹，并创建launch.json文件，文件内容如下:
	{
	    // Use IntelliSense to learn about possible attributes.
	    // Hover to view descriptions of existing attributes.
	    // For more information, visit: https://go.microsoft.com/fwlink/?linkid=830387
	    "version": "0.2.0",
	    "configurations": [
	        {
	            "name": "Redis Server",
	            "type": "lldb",
	            "request": "launch",
	            "program": "${workspaceFolder}/src/redis-server", // 确保路径正确指向redis-server的可执行文件
	            "args": ["./redis.conf"],
	            "cwd": "${workspaceFolder}",
	        }
	    ]
	}
5. 这样就可以启动debug了。
6. 用本地安装的redis-cli进行连接这个Redis服务器。