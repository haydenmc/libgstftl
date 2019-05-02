ftl_sources = [
  'gstftl.c',
  'gstftlaudiosink.c',
  'gstftlenums.c',
  'gstftlsink.c',
  'gstftlvideosink.c',
]

ftl_dep = dependency('libftl', version : '>= 0.5.0', required : get_option('ftl'))

if ftl_dep.found()
  gstftl = library('gstftl',
    ftl_sources,
    c_args : gst_plugins_bad_args,
    include_directories : [configinc],
    dependencies : [gstbase_dep, ftl_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  pkgconfig.generate(gstftl, install_dir : plugins_pkgconfig_install_dir)
  plugins += [gstftl]
endif
