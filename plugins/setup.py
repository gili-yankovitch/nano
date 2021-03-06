import setuptools

with open("README.md", "r") as fh:
    long_description = fh.read()

setuptools.setup(
    name="pynano",
    version="0.0.1",
    author="Gili Yankovitch",
    author_email="giliy@nyxsecurity.net",
    description="Python bindings for nano-python plugins extension",
    long_description=long_description,
    url="https://github.com/gili-yankovitch/nano/tree/plugins",
    packages=setuptools.find_packages(),
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
    ],
    python_requires='>=3.6',
)
