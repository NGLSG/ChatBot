import importlib
import importlib.metadata


def get_installed_packages():
    # 获取所有已安装的包
    installed_packages = [dist.metadata['Name'] for dist in importlib.metadata.distributions()]
    return installed_packages


print(get_installed_packages())
