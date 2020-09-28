import setuptools

with open("README.md", "r") as fh:
    long_description = fh.read()

setuptools.setup(
    name="nanotags",
    version="0.0.1",
    author="Gili Yankovitch",
    author_email="giliy@nyxsecurity.net",
    description="Nano-Editor plugin for C/C++ source browsing",
    long_description=long_description,
    url="https://github.com/gili-yankovitch/nano/tree/plugins",
    packages=setuptools.find_packages(),
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
    ],
    python_requires='>=3.6',
    install_requires=['pynano',
    				'clang'],
)
