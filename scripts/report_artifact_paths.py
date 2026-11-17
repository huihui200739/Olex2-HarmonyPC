"""Pure path planning for summaries; does not build, run, or alter evidence."""
from pathlib import PurePosixPath


def baseline_artifact_paths(previous_paths, build_directory, executable_names):
    build = PurePosixPath(build_directory)
    if build.is_absolute() or not build.parts or build.parts[0] != 'out' or len(build.parts) < 2 or '..' in build.parts:
        raise ValueError('Native artifacts require a relative project out directory')
    names = list(dict.fromkeys(executable_names))
    if any(not name.startswith('olex2_') or PurePosixPath(name).name != name for name in names):
        raise ValueError('Expected executable basenames only')
    paths = []
    for original in previous_paths:
        path = PurePosixPath(original)
        if path == PurePosixPath('out/app/entry-default-unsigned.hap'):
            paths.append(str(path))
        elif path.name in names or (path.name.startswith('libolex2_') and path.suffix == '.a'):
            if path.is_absolute() or '..' in path.parts or not path.parts or path.parts[0] != 'out':
                raise ValueError('Previous native artifact is outside the project out directory')
            paths.append(str(build / path.name))
        else:
            raise ValueError('Unknown baseline artifact: ' + str(path))
    paths.extend(str(build / name) for name in names)
    return list(dict.fromkeys(paths))
