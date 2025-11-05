import copy

class Params:
    """Object reorientation of a dictionary."""

    def __init__(self, **dict_):
        new_attrs = copy.deepcopy(dict_)
        self.__dict__.update(**new_attrs)

    def update(self, **dict_):
        p = Params(**self.__dict__)
        new_attrs = copy.deepcopy(dict_)
        p.__dict__.update(**new_attrs)
        return p


if __name__ == "__main__":
    pass
