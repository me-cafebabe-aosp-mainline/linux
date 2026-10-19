import sys,re
p=sys.argv[1]; s=open(p).read()
m=re.search(r'static int (\w+)_probe\(',s); n=m.group(1)
s=s.replace('''	ret = devm_drm_panel_add(dev, &ctx->panel);
	if (ret)
		return ret;

	return devm_mipi_dsi_attach(dev, dsi);
}
''',f'''	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {{
		drm_panel_remove(&ctx->panel);
		return dev_err_probe(dev, ret, "Failed to attach to DSI host\\n");
	}}

	return 0;
}}

static void {n}_remove(struct mipi_dsi_device *dsi)
{{
	struct {n} *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\\n", ret);

	drm_panel_remove(&ctx->panel);
}}
''')
s=s.replace(f'\t.probe = {n}_probe,\n',f'\t.probe = {n}_probe,\n\t.remove = {n}_remove,\n')
open(p,'w').write(s)
