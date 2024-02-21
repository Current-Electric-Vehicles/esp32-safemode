
Import("env")

web_build_type = env.GetProjectOption("web_build_type", "progmem")

if web_build_type == 'fs':
    env.AddCustomTarget(
        name="build_web",
        dependencies=None,
        actions=[
            "cd ../web && npm install && npm run build",
            "mkdir -p ./data/www",
            "rsync --archive --progress --stats --delete-after ../web/dist/ ./data/www/"
        ],
        title="Build Web",
        description="Build the web interface"
    )

elif web_build_type == 'progmem':
    env.AddCustomTarget(
        name="build_web",
        dependencies=None,
        actions=[
            "cd ../web && npm install && npm run awot-static"
        ],
        title="Build Web",
        description="Build the web interface"
    )
