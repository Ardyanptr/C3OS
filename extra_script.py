Import("env")
env.Replace(
    CC="ccache" + env.get("CC"),
    CXX="ccache" + env.get("CXX"),
    AR="ccache" + env.get("AR")
)