Note:
1.VS2015 sprintf -> sprintf_s
打开项目----项目属性---配置属性----C/C++ ----预处理器----预处理定义，
添加_CRT_SECURE_NO_DEPRECATE和_SCL_SECURE_NO_DEPRECATE这两个宏。 
ok，这样就不会提示了，要注意，这个配置的属性只是针对于当前项目，不对其他的项目有任何影响。 
