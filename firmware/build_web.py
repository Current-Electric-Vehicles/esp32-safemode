
Import("env")

def build_web_progmem(source, target, env):
    print("running build_web_progmem")
    PROJECT_DIR = env.subst("$PROJECT_DIR")
    env.Execute(f"npm --prefix={PROJECT_DIR}/../web install")
    env.Execute(f"npm --prefix={PROJECT_DIR}/../web run awot-static")

env.AddCustomTarget(
    name="build_web",
    dependencies=None,
    actions=[build_web_progmem],
    title="Build Web",
    description="Build the web interface"
)