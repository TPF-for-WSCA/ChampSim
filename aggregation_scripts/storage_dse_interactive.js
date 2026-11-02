(() => {
    const graph = document.getElementById('{plot_id}');
    // Keep the complete arrays: repeated changes must also restore hidden points.
    const original = graph.data.map(trace => ({
        x: Array.from(trace.x),
        y: Array.from(trace.y),
        customdata: trace.customdata.map(row => Array.from(row)),
        design: trace.meta.design,
        metric: trace.meta.metric
    }));
    const controls = document.createElement('div');
    controls.className = 'controls';
    controls.innerHTML = `
        <label>Nominal L1I <select id="icache-size" aria-label="Nominal L1I capacity"></select></label>
        <label>BTB entries <select id="btb-size" aria-label="BTB entry count"></select></label>
        <span id="selection-status" role="status" aria-live="polite"></span>`;
    graph.before(controls);
    const icache = controls.querySelector('#icache-size');
    const btb = controls.querySelector('#btb-size');
    const status = controls.querySelector('#selection-status');
    const metadata = original.filter(trace => trace.metric === 'ipc').flatMap(trace => trace.customdata);
    function options(select, column, unit) {
        select.add(new Option('All', 'all'));
        const sizes = [...new Set(metadata.map(row => row[column]))].sort((a, b) => a - b);
        sizes.forEach(size => select.add(new Option(`${size.toLocaleString()}${unit}`, String(size))));
    }
    options(icache, 1, ' KiB');
    options(btb, 2, ' entries');
    function update() {
        const selected = original.map(trace => {
            const indices = trace.customdata.map((row, index) => index).filter(index => {
                const row = trace.customdata[index];
                return (icache.value === 'all' || row[1] === Number(icache.value)) &&
                       (btb.value === 'all' || row[2] === Number(btb.value));
            });
            return {
                x: indices.map(i => trace.x[i]),
                y: indices.map(i => trace.y[i]),
                customdata: indices.map(i => trace.customdata[i])
            };
        });
        // Updating data only preserves the user's legend visibility and zoom.
        const pending = Plotly.restyle(graph, {
            x: selected.map(trace => trace.x),
            y: selected.map(trace => trace.y),
            customdata: selected.map(trace => trace.customdata)
        });
        const counts = original.filter(trace => trace.metric === 'ipc').map(trace => {
            const index = original.indexOf(trace);
            return `${graph.data[index].name}: ${selected[index].x.length}`;
        });
        status.textContent = `${counts.join(' · ')} point(s)`;
        return pending;
    }
    icache.addEventListener('change', update);
    btb.addEventListener('change', update);
    update();
})();
