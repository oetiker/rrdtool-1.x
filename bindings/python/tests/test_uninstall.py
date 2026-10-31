import csv
import importlib.util
import os
import shutil
import tempfile
import unittest
import zipfile


spec = importlib.util.spec_from_file_location(
    'rrdtool_uninstall', os.path.join(os.path.dirname(__file__), '..', 'uninstall.py'))
uninstaller = importlib.util.module_from_spec(spec)
spec.loader.exec_module(uninstaller)


class TestUninstall(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.mkdtemp(prefix='rrdtool-uninstall-')
        self.target = os.path.join(self.directory, 'site-packages')
        self.record = 'rrdtool-0.1.10.dist-info/RECORD'
        self.module = 'rrdtool.cpython-test.so'
        self.wheel = os.path.join(self.directory, 'rrdtool.whl')
        with zipfile.ZipFile(self.wheel, 'w') as archive:
            archive.writestr(self.record, '')
        self.write(self.module, 'installed extension')
        self.write('rrdtool_other/__init__.py', 'other package')

    def tearDown(self):
        shutil.rmtree(self.directory)

    def write(self, name, contents):
        path = os.path.join(self.target, name)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w') as fh:
            fh.write(contents)

    def installed_record(self, names):
        path = os.path.join(self.target, self.record)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w', newline='') as fh:
            csv.writer(fh).writerows((name, '', '') for name in names)

    def test_remove_only_recorded_files(self):
        self.write('rrdtool-0.1.10.dist-info/INSTALLER', 'pip')
        self.installed_record([self.module, self.record,
                               'rrdtool-0.1.10.dist-info/INSTALLER'])
        uninstaller.uninstall(self.target, self.wheel)
        self.assertFalse(os.path.exists(os.path.join(self.target, self.module)))
        self.assertFalse(os.path.exists(os.path.join(
            self.target, 'rrdtool-0.1.10.dist-info')))
        self.assertTrue(os.path.isfile(os.path.join(
            self.target, 'rrdtool_other/__init__.py')))
        uninstaller.uninstall(self.target, self.wheel)

    def test_leave_files_without_installed_record(self):
        uninstaller.uninstall(self.target, self.wheel)
        self.assertTrue(os.path.isfile(os.path.join(self.target, self.module)))

    def test_reject_outside_path_before_removing_files(self):
        outside = os.path.join(self.directory, 'outside')
        with open(outside, 'w') as fh:
            fh.write('not part of this installation')
        self.installed_record([self.module, '../outside', self.record])
        with self.assertRaises(ValueError):
            uninstaller.uninstall(self.target, self.wheel)
        self.assertTrue(os.path.isfile(outside))
        self.assertTrue(os.path.isfile(os.path.join(self.target, self.module)))


if __name__ == '__main__':
    unittest.main()
